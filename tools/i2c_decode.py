#!/usr/bin/env python3
"""
Разбор захвата I²C с логического анализатора DSLogic (экспорт libsigrok4DSL в CSV).

Зачем: при разборе «дисплей не показывает, хотя подключён верно» нужно отделить
проблему шины от проблемы самого модуля. Скрипт декодирует захват в транзакции,
показывает, какие адреса опрашиваются и кто отвечает ACK, а для модуля LCD1602 на
PCF8574 дополнительно восстанавливает команды и текст, которые прошивка в него пишет,
с разбивкой по пачкам активности (инициализация при старте идёт отдельной пачкой
от периодических обновлений экрана).

Запуск:
    python tools/i2c_decode.py "dslogick/DSLogic U2Basic-la-261016-112742.csv"
    python tools/i2c_decode.py capture.csv --scl 1 --sda 0
    python tools/i2c_decode.py capture.csv --raw        # без разбора LCD1602

Файл читается потоково в два прохода, поэтому захваты на десятки миллионов сэмплов
разбираются без больших затрат памяти.

Зависимостей нет, только стандартная библиотека.
"""

from __future__ import annotations

import argparse
import sys
from collections import Counter

# Распространённая разводка платы-переходника LCD1602 на PCF8574 («синяя» с подстроечником):
# P0=RS, P1=RW, P2=EN, P3=подсветка, P4..P7=D4..D7.
BACKPACK_RS = 0x01
BACKPACK_EN = 0x04
BACKPACK_BL = 0x08

# Пауза между кадрами, после которой начинается новая «пачка» активности.
BURST_GAP_SAMPLES_1MHZ = 20_000


def read_header(path: str) -> tuple[int, int, int]:
    """Возвращает (частота дискретизации, число каналов, заявленное число сэмплов)."""
    rate, channels, declared = 1_000_000, 0, 0
    with open(path, newline="", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if line.startswith(";"):
                if "Sample rate:" in line:
                    value = line.split(":", 1)[1].strip()
                    number = "".join(ch for ch in value if ch.isdigit() or ch == ".")
                    factor = 1
                    if "kHz" in value or "KHz" in value:
                        factor = 1_000
                    elif "MHz" in value:
                        factor = 1_000_000
                    try:
                        rate = int(float(number) * factor)
                    except ValueError:
                        pass
                elif "Channels (" in line:
                    # ; Channels (2/16) — первое число: сколько каналов в захвате.
                    inner = line.split("(", 1)[1].split(")", 1)[0].split("/", 1)[0].strip()
                    channels = int(inner) if inner.isdigit() else 0
                elif "Sample count:" in line:
                    # ; Sample count: 10.384 M Samples
                    value = line.split(":", 1)[1].strip()
                    number = "".join(ch for ch in value if ch.isdigit() or ch == ".")
                    factor = 1
                    if "k" in value:
                        factor = 1_000
                    elif "M" in value:
                        factor = 1_000_000
                    try:
                        declared = int(float(number) * factor)
                    except ValueError:
                        declared = 0
                continue
            if line.startswith("Time"):
                header = [part.strip() for part in line.strip().split(",")]
                channels = max(channels, len(header) - 1)
                break
            if line.strip():
                break
    return rate, channels, declared


def channel_stats(path: str) -> tuple[dict[int, dict], int]:
    """Один проход: число фронтов, уровни в покое и самый длинный низкий уровень по каналам."""
    stats: dict[int, dict] = {}
    samples = 0
    previous: list[str | None] = []
    with open(path, newline="", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line or line[0] in ";T\n":
                continue
            parts = line.rstrip("\r\n").split(",")
            if len(parts) < 2 or not parts[0][:1].isdigit():
                continue
            values = parts[1:]
            while len(previous) < len(values):
                previous.append(None)
            while len(stats) < len(values):
                stats[len(stats)] = {"transitions": 0, "first": None, "last": None, "longest_low": 0, "low_run": 0}
            for index, value in enumerate(values):
                entry = stats[index]
                if entry["first"] is None:
                    entry["first"] = value
                if previous[index] is not None and previous[index] != value:
                    entry["transitions"] += 1
                if value == "0":
                    entry["low_run"] += 1
                    if entry["low_run"] > entry["longest_low"]:
                        entry["longest_low"] = entry["low_run"]
                else:
                    entry["low_run"] = 0
                entry["last"] = value
                previous[index] = value
            samples += 1
    return stats, samples


def decode_frames(path: str, scl_channel: int, sda_channel: int) -> list[dict]:
    """Потоковое декодирование I²C. bytes[0] — байт адреса с битом R/W, далее данные."""
    frames: list[dict] = []
    current: dict | None = None
    bit_index = 0
    value = 0
    index = -1
    previous_scl = previous_sda = None
    maxsplit = max(scl_channel, sda_channel) + 1
    with open(path, newline="", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line or line[0] in ";T" or line[0] == "\n":
                continue
            parts = line.rstrip("\r\n").split(",", maxsplit)
            if len(parts) <= maxsplit:
                continue
            index += 1
            cur_scl = parts[1 + scl_channel]
            cur_sda = parts[1 + sda_channel]
            if previous_scl is not None:
                if previous_sda == "1" and cur_sda == "0" and cur_scl == "1" and previous_scl == "1":
                    if current is not None:
                        current["end"] = index
                        current["stop"] = "restart"
                        frames.append(current)
                    current = {"start": index, "end": None, "bytes": [], "acks": [], "stop": "none"}
                    bit_index = 0
                    value = 0
                elif previous_sda == "0" and cur_sda == "1" and cur_scl == "1" and previous_scl == "1":
                    if current is not None:
                        current["end"] = index
                        current["stop"] = "stop"
                        frames.append(current)
                        current = None
                elif previous_scl == "0" and cur_scl == "1" and current is not None:
                    if bit_index < 8:
                        value = (value << 1) | (1 if cur_sda == "1" else 0)
                        bit_index += 1
                    else:
                        current["bytes"].append(value)
                        current["acks"].append("ACK" if cur_sda == "0" else "NACK")
                        bit_index = 0
                        value = 0
            previous_scl, previous_sda = cur_scl, cur_sda
    if current is not None:
        current["end"] = index
        frames.append(current)
    return frames


def estimate_frequency(frames: list[dict], sample_rate: int) -> float | None:
    spans = [(frame["bytes"], frame["end"] - frame["start"])
             for frame in frames
             if frame["end"] is not None and len(frame["bytes"]) >= 1]
    if not spans:
        return None
    # Полный кадр из N байт занимает (N*9 + 2) тактов шины.
    ratios = [duration / (len(data) * 9 + 2) for data, duration in spans if duration > 0]
    if not ratios:
        return None
    ratios.sort()
    median = ratios[len(ratios) // 2]
    return sample_rate / median if median > 0 else None


def group_bursts(frames: list[dict], sample_rate: int) -> list[list[dict]]:
    gap = BURST_GAP_SAMPLES_1MHZ * sample_rate // 1_000_000
    bursts: list[list[dict]] = []
    for frame in frames:
        if bursts and frame["start"] - bursts[-1][-1]["end"] <= gap:
            bursts[-1].append(frame)
        else:
            bursts.append([frame])
    return bursts


def describe_burst(burst: list[dict], sample_rate: int) -> str:
    devices = Counter()
    for frame in burst:
        if frame["bytes"]:
            devices[(frame["bytes"][0] >> 1, "R" if frame["bytes"][0] & 1 else "W")] += 1
    listed = ", ".join(f"0x{address:02X}{direction}×{count}" for (address, direction), count in devices.most_common())
    duration_ms = (burst[-1]["end"] - burst[0]["start"]) * 1000.0 / sample_rate
    start_ms = burst[0]["start"] * 1000.0 / sample_rate
    return f"  {start_ms:>9.2f} мс: кадров {len(burst):>4}, длительность {duration_ms:>7.2f} мс, {listed}"


def decode_backpack(burst: list[dict], address: int, sample_rate: int, indent: str = "    ") -> None:
    """Собирает команды и текст из одиночных байтовых записей в PCF8574.

    Один полубайт HD44780 в этой схеме — три записи байта: значение, значение с EN=1,
    значение с EN=0.
    """
    writes = []
    for frame in burst:
        if len(frame["bytes"]) >= 2 and (frame["bytes"][0] >> 1) == address:
            writes.append((frame["start"], frame["bytes"][1], frame["end"]))

    commands: list[int] = []
    text: list[str] = []
    pending: int | None = None
    en_widths_us: list[float] = []
    index = 0
    while index + 2 < len(writes):
        first = writes[index][1]
        second_start, second, second_end = writes[index + 1]
        third = writes[index + 2][1]
        if second == (first | BACKPACK_EN) and third == first:
            nibble = (first >> 4) & 0x0F
            is_data = bool(first & BACKPACK_RS)
            if second_end is not None:
                en_widths_us.append((second_end - second_start) * 1_000_000.0 / sample_rate)
            if pending is None:
                pending = nibble
            else:
                assembled = (pending << 4) | nibble
                pending = None
                if is_data:
                    text.append(chr(assembled))
                else:
                    commands.append(assembled)
            index += 3
        else:
            index += 1

    if commands:
        listed = ", ".join(f"0x{command:02X}" for command in commands)
        print(f"{indent}команды (RS=0): {listed}")
    if text:
        print(f"{indent}текст: {''.join(text)!r}")
    if en_widths_us:
        print(
            f"{indent}EN=1: {min(en_widths_us):.0f}..{max(en_widths_us):.0f} мкс "
            f"(минимум по даташиту 0.45 мкс)"
        )
    if not commands and not text:
        print(f"{indent}одиночных записей в PCF8574 не найдено (только адресные пробы)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Разбор I²C-захвата DSLogic (CSV) для диагностики шины и LCD1602")
    parser.add_argument("capture", help="путь к CSV, экспортированному DSView/libsigrok")
    parser.add_argument("--scl", type=int, default=None, help="номер канала SCL (по умолчанию определяется автоматически)")
    parser.add_argument("--sda", type=int, default=None, help="номер канала SDA (по умолчанию определяется автоматически)")
    parser.add_argument("--raw", action="store_true", help="не разбирать поток LCD1602, показать только транзакции шины")
    parser.add_argument("--bursts", type=int, default=8, help="сколько пачек активности показать (по умолчанию 8)")
    args = parser.parse_args()

    rate, channel_count, declared = read_header(args.capture)
    stats, samples = channel_stats(args.capture)
    if not stats:
        print("Не удалось прочитать ни одного сэмпла", file=sys.stderr)
        return 2

    print(f"Файл: {args.capture}")
    print(f"Частота дискретизации: {rate} Гц, каналов {channel_count}, сэмплов {samples} "
          f"({samples / rate:.3f} с" + (f", заявлено {declared})" if declared else ")"))
    print("Активность каналов:")
    for number in sorted(stats):
        entry = stats[number]
        if entry["transitions"]:
            print(f"  ch{number}: фронтов {entry['transitions']}, самый длинный низкий уровень "
                  f"{entry['longest_low']} мкс")
        else:
            print(f"  ch{number}: без изменений (постоянный {entry['first']})")

    scl_channel, sda_channel = args.scl, args.sda
    active = sorted((entry["transitions"], number) for number, entry in stats.items() if entry["transitions"])
    if scl_channel is None or sda_channel is None:
        if len(active) < 2:
            print("В захвате меньше двух переключающихся каналов — задайте --scl/--sda", file=sys.stderr)
            return 2
        # Больше всего фронтов — тактирование; второй по активности — данные.
        if scl_channel is None:
            scl_channel = active[-1][1]
        if sda_channel is None:
            sda_channel = active[-2][1]
    if scl_channel == sda_channel:
        print("Каналы SCL и SDA совпали — задайте их явно", file=sys.stderr)
        return 2

    print(f"\nКаналы: SCL = ch{scl_channel}, SDA = ch{sda_channel}")
    print(f"Уровни в покое: начало ch{scl_channel}={stats[scl_channel]['first']}/ch{sda_channel}={stats[sda_channel]['first']}, "
          f"конец ch{scl_channel}={stats[scl_channel]['last']}/ch{sda_channel}={stats[sda_channel]['last']}")

    frames = decode_frames(args.capture, scl_channel, sda_channel)
    frequency = estimate_frequency(frames, rate)
    if frequency:
        print(f"Оценка частоты SCL: ~{frequency / 1000:.0f} кГц")

    failed = [frame for frame in frames if "NACK" in frame["acks"]]
    print("\n== Транзакции ==")
    print(f"  кадров: {len(frames)}, из них с NACK: {len(failed)}")

    devices: Counter = Counter()
    for frame in frames:
        if not frame["bytes"]:
            continue
        first = frame["bytes"][0]
        devices[(first >> 1, "R" if first & 1 else "W", frame["acks"][0] if frame["acks"] else "?")] += 1
    print("  обращения мастера (адрес, R/W, ответ устройства):")
    for (address, direction, answer), count in sorted(devices.items(), key=lambda item: -item[1]):
        print(f"    0x{address:02X} {direction}  {answer:<5} ×{count}")

    probed = sorted({frame["bytes"][0] >> 1 for frame in frames if len(frame["bytes"]) == 1})
    if probed:
        print("  адресные пробы без данных: " + ", ".join(f"0x{address:02X}" for address in probed))

    bursts = group_bursts(frames, rate)
    print(f"\n== Пачки активности (пауза между пачками > "
          f"{BURST_GAP_SAMPLES_1MHZ * 1000.0 / 1_000_000:.0f} мс) ==")
    print(f"  всего пачек: {len(bursts)}")
    for burst in bursts[:args.bursts]:
        print(describe_burst(burst, rate))
    if len(bursts) > args.bursts:
        print(f"  ... и ещё {len(bursts) - args.bursts}")

    if not args.raw:
        answered = sorted({address for (address, _direction, answer), _count in devices.items() if answer == "ACK"})
        for address in answered:
            if address not in (0x27, 0x3F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E):
                continue
            print(f"\n== Разбор потока в модуль по адресу 0x{address:02X} ==")
            for number, burst in enumerate(bursts[:args.bursts]):
                writes = sum(1 for frame in burst if len(frame["bytes"]) >= 2 and (frame["bytes"][0] >> 1) == address)
                if writes < 3:
                    continue
                start_ms = burst[0]["start"] * 1000.0 / rate
                print(f"  пачка {number + 1} (с {start_ms:.2f} мс, записей {writes}):")
                decode_backpack(burst, address, rate)

    if failed:
        print("\nВНИМАНИЕ: есть NACK — устройство не подтвердило обращение.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
