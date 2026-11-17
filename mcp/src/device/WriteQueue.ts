/**
 * Один писатель на устройство (контракт §3.4/§3.5).
 *
 * ESP32 не должна получать конкурирующие команды от двух агентов, поэтому все
 * изменяющие вызовы выстраиваются в очередь и выполняются строго последовательно.
 * Чтения через очередь не идут: их схлопывает single-flight в DeviceClient.
 *
 * Дедупликации «одинаковый вызов в пределах 1 с» здесь нет намеренно: ключ
 * `withWrite(key, …)` не содержит аргументов, и дедуп по ключу склеил бы,
 * например, `start` и `stop` одного домена. Дедуп делает домен, который знает
 * сигнатуру действия (см. отчёт WP0, contract_changes).
 */

import type { Logger } from '../logging.js';

export class WriteQueue {
  private tail: Promise<unknown> = Promise.resolve();
  private waiting = 0;
  private readonly log?: Logger;
  private completed = 0;

  constructor(log?: Logger) {
    this.log = log;
  }

  /** Сколько операций ещё не завершилось (включая выполняющуюся). */
  get size(): number {
    return this.waiting;
  }

  get completedCount(): number {
    return this.completed;
  }

  /** Ставит операцию в очередь; ошибки не рвут очередь для следующих. */
  run<T>(key: string, fn: () => Promise<T>): Promise<T> {
    this.waiting += 1;
    const task = this.tail.then(async () => {
      try {
        return await fn();
      } finally {
        this.waiting -= 1;
        this.completed += 1;
      }
    });
    // Хвост очереди не должен «запоминать» ошибку предыдущей операции.
    this.tail = task.then(
      () => undefined,
      (error: unknown) => {
        this.log?.debug('Операция записи завершилась ошибкой', { key, error });
        return undefined;
      },
    );
    return task;
  }

  /** Дожидается опустошения очереди (для корректного завершения и тестов). */
  async drain(): Promise<void> {
    await this.tail;
  }
}
