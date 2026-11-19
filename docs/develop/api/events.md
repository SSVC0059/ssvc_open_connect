# События WebSocket (`/ws/events`)

Канал push-событий прошивки: клиент один раз подписывается и получает изменения
состояния кадрами, вместо того чтобы опрашивать REST. Сервер — тот же
`AsyncWebServer`, что и REST; путь — `/ws/events`.

| Что | Значение |
|-----|----------|
| Путь | `/ws/events` |
| Транспорт | WebSocket поверх HTTP (TLS нет: `ws://`, не `wss://`) |
| Формат кадров | **бинарный MessagePack** (в этой сборке `EVENT_USE_JSON=0`) |
| Аутентификация | при рукопожатии, `?access_token=<JWT>`; на сборке без `FT_SECURITY` токен не нужен |
| Права | только администратор (`IS_ADMIN`): канал отдаёт те же данные, что admin-only REST |
| Подписка | обязательна: без `{"event":"subscribe",...}` событий не будет |

## Чем отличается от REST-опроса

| | REST (`/rest/telemetry`, `/rest/oc/*`) | `/ws/events` |
|---|---|---|
| Инициатор | клиент, каждый раз | устройство, при изменении |
| Задержка | до 2 с (квантование телеметрии) + сеть | сразу после изменения |
| Формат | JSON | MessagePack (бинарный) |
| Аутентификация | заголовок `Authorization` на каждый запрос | один раз при подключении |
| Гарантия доставки | ответ на запрос | **нет**: пропущенное не повторяется |
| `telemetry` | `GET /rest/telemetry` (admin) | то же тело, тоже только admin |

Смысл канала — не заменять REST, а избавить клиента (MCP-сервер, UI) от
постоянного опроса. Начальное состояние по-прежнему удобно брать из REST
(например, `GET /rest/oc/snapshot`), а дальше слушать события.

## Формат кадра

Обе стороны шлют **один и тот же конверт**: объект с ключами `event` и `data`.

Клиент → устройство:

```json
{"event": "subscribe",   "data": "telemetry"}
{"event": "unsubscribe", "data": "telemetry"}
```

Устройство → клиент:

```json
{"event": "telemetry", "data": { /* ... */ }}
```

* `data` при подписке — **строка** с именем события, а не объект.
* Подписаться можно только на зарегистрированное событие; неизвестное имя
  игнорируется (в лог устройства уходит warning), подписки не появляется.
* `unsubscribe` снимает подписку; при обрыве соединения подписки клиента
  удаляются автоматически.
* В этой сборке кадры бинарные. Клиент, отправивший текстовый JSON, будет
  проигнорирован — отправлять подписку нужно тоже MessagePack (см. пример ниже).
  Если прошивку собрать с `EVENT_USE_JSON=1`, конверт тот же, но кадры текстовые.

`origin`: события, вызванные действием конкретного клиента, могут не отправляться
инициатору. Для `telemetry`/`stage`/`alarm` это не используется — их получают все
подписчики, кроме случая догоняющей отправки при подписке (тогда кадр уходит
только новому подписчику).

## События

### `telemetry`

Полный снимок состояния — **ровно тело `GET /rest/telemetry`**, включая
`lastUpdate`. Клиенту не нужно разбирать два разных формата.

Условие эмиссии: изменилось состояние телеметрии. Внутренний таймер пересобирает
снимок раз в 2 с, но кадр уходит только если сериализованный снимок отличается от
предыдущего, то есть **не чаще ~1 раза в 2 с** и не «каждые 2 с» при неизменном
состоянии.

При подписке устройство сразу отправляет текущий снимок этому подписчику
(догоняющая отправка) — ждать следующего изменения не нужно.

```json
{
  "event": "telemetry",
  "data": {
    "telemetry": {
      "type": "heads",
      "pid": 120,
      "start_time": "17.11.2026 14:02:11",
      "countdown": "00:41:03",
      "time": "00:12:57",
      "tank_mmhg": 12.4,
      "tp1_target": 78.6,
      "tp2_target": 77.9,
      "open": 1.6,
      "period": 3,
      "valveOpen": 5333,
      "volumeSpeed": 21,
      "v1": 1,
      "v2": 0,
      "stop": false,
      "stops": 0,
      "common": {
        "mmhg": 12.4,
        "tp1": 78.6,
        "tp2": 77.9,
        "relay": true,
        "signal": false,
        "cfg_chgd": true,
        "tts": 12,
        "heatingOn": true,
        "overclockingOn": false,
        "relative_pressure": 3.1
      },
      "hysteresis": 0.25,
      "volume": { "heads": 480, "hearts": 0, "late_heads": 0 },
      "alc": 0.0
    },
    "status": {
      "stage": "heads",
      "status": "running",
      "startTime": "17.11.2026 14:02:11",
      "endTime": "",
      "stages": { "waiting": "finished", "heads": "running" },
      "uartConnectionError": false
    },
    "lastUpdate": 918452
  }
}
```

`telemetry` условна: часть полей (`pid`, `tp1_target`, `open`, `v1`…) появляется
только когда значение есть; `volume` содержит либо `tails`, либо `late_heads`.
Полный список условий — в `GET /rest/telemetry`. `lastUpdate` — `millis()`
устройства, а не Unix-время.

### `stage`

Смена стадии, статуса процесса, события или PID. Эмитится **только на переход**;
периодических кадров нет.

| Поле | Тип | Значение |
|------|-----|----------|
| `stage` | string | `empty`, `waiting`, `tp1_waiting`, `delayed_start`, `heads`, `late_heads`, `hearts`, `tails`, `error`, `unknown` — те же строки, что в `status.stage` из `/rest/telemetry` |
| `status` | string | `idle`, `running`, `paused`, `finished`, `skipped`, `unknown` — как `status.status` |
| `event` | string | имя события (`heads_finished`, `hearts_finished`, `tails_finished`, `ds_error`, `ds_error_stop`, `stabilization_limit`, `remote_stop`, `manually_closed`, `manually_opened`); **пустая строка**, если события нет (в REST поле `telemetry.event` в этом случае отсутствует) |
| `pid` | number | PID процесса, `0` — процесса нет |
| `observed_at` | number | `millis()` устройства на момент отправки, не epoch |

```json
{
  "event": "stage",
  "data": { "stage": "heads", "status": "running", "event": "", "pid": 120, "observed_at": 918452 }
}
```

Событие процесса:

```json
{
  "event": "stage",
  "data": { "stage": "hearts", "status": "paused", "event": "manually_closed", "pid": 120, "observed_at": 918930 }
}
```

Догоняющая отправка при подписке: новый подписчик сразу получает текущие
`stage/status/event/pid`. Базовое состояние до первого кадра UART
(`empty`/`idle`, `pid:0`) отдельным событием не считается.

### `alarm`

Тревога датчика (`kind:"threshold"`) или аппаратный сбой (`kind:"hardware"`).
Эмитится только на переход: появление, снятие, смена уровня (для датчика) или
кода сбоя (для hardware). Повторный опрос, вернувший то же состояние, кадр не
порождает.

| Поле | Тип | Значение |
|------|-----|----------|
| `kind` | string | `threshold` — тревога датчика; `hardware` — аппаратный сбой |
| `level` | string | `min`, `dangerous`, `critical`, `normal` (снятие тревоги) |
| `address` | string \| null | адрес датчика, 16 hex-символов в верхнем регистре; `null` для hardware |
| `zone` | string \| null | зона датчика (`tank`, `inlet_water`, `receiver_tank_1`, …); `null` для hardware |
| `role` | string \| null | роль устройства для hardware (`ssvc_uart`, `i2c_bus`, `bmp581`, `lcd1602`, `pcf8574_port0`…); `null` для датчика |
| `code` | string \| null | для hardware — `i2c_nack`, `i2c_bus_down`, `device_not_present`, `uart_link_lost`; `null` при снятии и для датчика |
| `active` | bool | `true` — тревога/сбой активны, `false` — сняты |
| `observed_at` | number | `millis()` устройства, не epoch |

Тревога датчика:

```json
{
  "event": "alarm",
  "data": {
    "kind": "threshold",
    "level": "dangerous",
    "address": "28FF64A1B2C3D4E5",
    "zone": "tank",
    "role": null,
    "code": null,
    "active": true,
    "observed_at": 918452
  }
}
```

Её снятие:

```json
{
  "event": "alarm",
  "data": {
    "kind": "threshold",
    "level": "normal",
    "address": "28FF64A1B2C3D4E5",
    "zone": "tank",
    "role": null,
    "code": null,
    "active": false,
    "observed_at": 923004
  }
}
```

Аппаратный сбой:

```json
{
  "event": "alarm",
  "data": {
    "kind": "hardware",
    "level": "critical",
    "address": null,
    "zone": null,
    "role": "ssvc_uart",
    "code": "uart_link_lost",
    "active": true,
    "observed_at": 918452
  }
}
```

Снятие сбоя — те же `role`, `active:false`, `level:"normal"`, `code:null`.

Смена уровня (`min` → `dangerous` → `critical`) — отдельное событие; смена кода
сбоя при том же уровне (например, `i2c_nack` → `i2c_bus_down`) — тоже.

## Частота и стоимость кадров

| Событие | Когда | Типичный кадр (MessagePack) |
|---|---|---|
| `telemetry` | при изменении снимка, не чаще ~1 раза в 2 с; плюс один кадр при подписке | ≈ 0,4–0,6 КБ (зависит от набора полей; пример выше — ≈ 556 Б) |
| `stage` | при смене `stage`/`status`/`event`/`pid`; плюс один кадр при подписке | ≈ 75 Б (пример выше) |
| `alarm` | при переходе тревоги/сбоя | ≈ 120–125 Б (примеры выше) |

Размеры указаны для конверта целиком (`event` + `data`) при `EVENT_USE_JSON=0`.
В JSON-режиме кадр примерно на 30–40 % больше (пример телеграммы: 721 Б).

## Ограничения

* **Нет гарантии доставки.** Кадры не буферизуются и не повторяются: пока
  клиент отключён, события теряются. Отдельной очереди и истории на устройстве
  нет. После переподключения нужно подписаться заново.
* **Догоняющая отправка есть только у `telemetry` и `stage`** (`onSubscribe`).
  У `alarm` её нет: текущие аппаратные сбои читаются через
  `GET /rest/oc/hardware-faults`, состояние тревог датчиков — из `GET /rest/telemetry`
  (значения) и порогов; если тревога началась/кончилась, пока клиент был
  отключён, отдельного «снимка тревог» не будет.
* **Эмиссия не из loop-задачи.** `telemetry` отправляется из задачи таймера
  FreeRTOS (или из WS-задачи при подписке), `stage` — из задачи `RectTelemetry`
  (ядро 1), `alarm` — из задач опроса датчиков, I²C/UART и обработчиков REST.
  Список подписок в `EventSocket` защищён мьютексом, но вызов
  `AsyncWebSocket::binary()` из чужой задачи библиотекой официально не
  гарантирован. На практике работает; при интенсивной нагрузке на устройстве
  это надо проверять отдельно.
* **Переполнение буфера клиента.** Кадр сериализуется целиком в буфер и
  отправляется синхронно; если TCP-буфер клиента переполнен (медленный или
  зависший получатель), кадр может быть потерян. Ограничения на размер кадра
  нет, но `telemetry` — самый тяжёлый.
* **Стоимость без подписчиков.** `EventSocket` сериализует кадр даже при пустом
  списке подписок. Прошивка проверяет число подключённых клиентов и не собирает
  кадр, когда не подключён никто, но если клиент подключён без подписки на
  событие, сериализация всё равно произойдёт.
* **Память.** `telemetry` собирается в PSRAM (при её отсутствии — внутренняя
  куча) и на каждый кадр дополнительно выделяет буфер под сериализацию плюс
  копию на каждого подписчика. Поэтому эмиссия привязана к изменению, а не к
  таймеру. `stage` и `alarm` — маленькие кадры на внутренней куче.
* **Права.** WebSocket-фильтр — `IS_ADMIN`: канал отдаёт `telemetry`, `stage` и
  `alarm` — те же данные, что admin-only `GET /rest/telemetry` и `/rest/sensor`.
  Гость (не admin) получает `401` на рукопожатии, как и на admin-маршрутах REST;
  отдельного гостевого канала нет.
* **`observed_at`/`lastUpdate` — не время.** Это `millis()` с загрузки
  устройства (переполнение ~49,7 сут). Для привязки к настенному времени нужен
  NTP-синхронизированный источник.

## Как проверить в браузере

Откройте консоль на странице UI устройства (или `http://DEVICE_IP/`), подставьте
токен и выполните сниппет: он подписывается на все три события и печатает
декодированные кадры. Токен берётся из `localStorage` после входа в UI либо через
`POST /rest/signIn`; на сборке без `FT_SECURITY` токен не нужен — можно передать
пустую строку.

```html
<script type="module">
  // MessagePack: в этой сборке и подписка, и события — бинарные кадры.
  import { encode, decode } from 'https://esm.sh/@msgpack/msgpack@3';

  const token = '<JWT>'; // на сборке без FT_SECURITY можно оставить пустым
  const ws = new WebSocket(
    `ws://${location.host}/ws/events?access_token=${encodeURIComponent(token)}`
  );
  ws.binaryType = 'arraybuffer';

  ws.onopen = () => {
    for (const name of ['telemetry', 'stage', 'alarm']) {
      ws.send(encode({ event: 'subscribe', data: name }));
    }
    console.log('subscribed');
  };

  ws.onmessage = (event) => {
    const frame = decode(new Uint8Array(event.data));
    console.log(frame.event, frame.data);
  };

  ws.onerror = (e) => console.error('ws error', e);
  ws.onclose = () => console.log('ws closed');
</script>
```

Проверка без браузера (CLI): удобнее всего `websocat`/`wscat` с бинарным вводом,
но проще написать маленький скрипт на Node.js с `ws` и `@msgpack/msgpack`:
подключиться к `ws://DEVICE_IP/ws/events?access_token=...`, отправить
`msgpack.encode({event:'subscribe',data:'telemetry'})`, декодировать ответы.
Первый же кадр после подписки — догоняющий снимок `telemetry`.

Ожидаемое поведение:

1. сразу после `subscribe` приходят `telemetry` (полный снимок) и `stage`
   (текущая стадия); `alarm` — только если в этот момент есть переход;
2. при работе процесса `stage` приходит на каждой смене стадии, `telemetry` —
   при изменении данных (не чаще ~1 раза в 2 с);
3. при отключении датчика/тревоге приходит `alarm` с `active:true`, при
   восстановлении — с `active:false`.
