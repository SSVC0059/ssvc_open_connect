#!/usr/bin/python3
import re
import sys
import os
import argparse
import glob
import subprocess
from construct import (Bytes, Int32ul, Struct)
from esp_coredump.corefile.elf import ESPCoreDumpElfFile
from esp_coredump.corefile.loader import ESPCoreDumpFileLoader
from esp_coredump.corefile import SUPPORTED_TARGETS
from esp_coredump.corefile.gdb import DEFAULT_GDB_TIMEOUT_SEC
from esp_coredump import CoreDump,__version__

try:
    from esptool.loader import ESPLoader
except (AttributeError, ModuleNotFoundError):
    from esptool import ESPLoader

def arg_auto_int(x):
    return int(x, 0)

def setup_esp_idf_environment(ps_profile_path):
    """Настраивает окружение ESP-IDF с помощью PowerShell профиля"""
    if not ps_profile_path or not os.path.exists(ps_profile_path):
        print(f"ВНИМАНИЕ: Файл профиля PowerShell не найден: {ps_profile_path}")
        return False

    print(f"Инициализация ESP-IDF с помощью профиля: {ps_profile_path}")

    try:
        # Запускаем PowerShell скрипт для настройки окружения
        cmd = [
            "powershell.exe",
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-Command",
            f". '{ps_profile_path}'; $env:PATH"
        ]

        result = subprocess.run(cmd, capture_output=True, text=True, timeout=15)

        if result.returncode == 0:
            print("✓ Окружение ESP-IDF успешно инициализировано")

            # Обновляем PATH в текущем процессе Python
            path_lines = result.stdout.strip().split('\n')
            if path_lines:
                # Последняя строка обычно содержит обновленный PATH
                new_path = path_lines[-1]
                os.environ['PATH'] = new_path
                print(f"✓ PATH обновлен ({len(new_path.split(';'))} элементов)")

            return True
        else:
            print(f"✗ Ошибка при инициализации ESP-IDF: {result.stderr}")
            return False

    except subprocess.TimeoutExpired:
        print("✗ Таймаут при инициализации ESP-IDF")
        return False
    except Exception as e:
        print(f"✗ Ошибка при запуске PowerShell: {e}")
        return False

def find_elf_by_partial_hash(partial_hash, search_dirs=None):
    """Ищет ELF файл по частичному хэшу SHA256 в имени файла"""
    if search_dirs is None:
        search_dirs = ['.', 'build', 'build/elf', 'build/elf/esp32-s3-devkitc-1-16m']

    all_elf_files = []

    # Ищем все ELF файлы в указанных директориях
    for search_dir in search_dirs:
        if os.path.isdir(search_dir):
            # Рекурсивный поиск
            pattern = os.path.join(search_dir, "**", "*.elf")
            elf_files = glob.glob(pattern, recursive=True)
            all_elf_files.extend(elf_files)

            # Не рекурсивный поиск
            pattern = os.path.join(search_dir, "*.elf")
            elf_files = glob.glob(pattern, recursive=False)
            all_elf_files.extend(elf_files)

    # Удаляем дубликаты
    all_elf_files = list(set(all_elf_files))

    print(f"Найдено {len(all_elf_files)} ELF файлов")

    # Ищем файлы, содержащие частичный хэш в имени
    matching_files = []
    for elf_file in all_elf_files:
        filename = os.path.basename(elf_file)
        filename_no_ext = os.path.splitext(filename)[0]

        # Проверяем, содержит ли имя файла наш частичный хэш
        if partial_hash.lower() in filename_no_ext.lower():
            matching_files.append(elf_file)

    return matching_files

def find_gdb_in_esp_idf():
    """Автоматически ищет GDB в типичных местах установки ESP-IDF"""
    possible_paths = [
        # Windows типичные пути ESP-IDF
        "C:\\Espressif\\tools\\xtensa-esp32s3-elf\\esp-*\\xtensa-esp32s3-elf\\bin\\xtensa-esp32s3-elf-gdb.exe",
        "C:\\Espressif\\tools\\riscv32-esp-elf\\esp-*\\riscv32-esp-elf\\bin\\riscv32-esp-elf-gdb.exe",
        os.path.expanduser("~/.espressif/tools/**/*gdb*.exe"),

        # Также ищем в PATH
        "gdb", "xtensa-esp32s3-elf-gdb", "riscv32-esp-elf-gdb"
    ]

    for pattern in possible_paths:
        try:
            files = glob.glob(pattern, recursive=True)
            for file in files:
                if 'gdb' in os.path.basename(file).lower() and os.path.isfile(file):
                    print(f"Найден GDB: {file}")
                    return file
        except:
            continue

    return None

# ====== ИНИЦИАЛИЗАЦИЯ ESP-IDF В САМОМ НАЧАЛЕ ======
print("=" * 60)
print("ИНИЦИАЛИЗАЦИЯ ESP-IDF ОКРУЖЕНИЯ")
print("=" * 60)

# Стандартный путь к профилю PowerShell ESP-IDF
PS_PROFILE_PATH = "C:\\Espressif\\tools\\Microsoft.v5.5.2.PowerShell_profile.ps1"

# Проверяем существование профиля
if not os.path.exists(PS_PROFILE_PATH):
    print(f"⚠ ВНИМАНИЕ: Файл профиля не найден: {PS_PROFILE_PATH}")
    print("  Убедитесь, что ESP-IDF правильно установлен.")
    print("  Вы можете указать другой путь с помощью --ps-profile")
else:
    # Выполняем инициализацию ESP-IDF
    setup_success = setup_esp_idf_environment(PS_PROFILE_PATH)

    if not setup_success:
        print("\n⚠ Продолжаю без полной инициализации ESP-IDF...")
        print("  Некоторые функции могут быть недоступны.")

print("\n" + "=" * 60)

parser = argparse.ArgumentParser(description=f'coredump.py - ESP32 Core Dump Utility')
parser.add_argument('--chip', default='auto', choices=['auto'] + SUPPORTED_TARGETS, help='Target chip type')
parser.add_argument('--port', '-p', help='Serial port device')
parser.add_argument('--baud', '-b', type=int, default=115200, help='Serial port baud rate')
parser.add_argument('--prog', help='Path to application ELF file')
parser.add_argument('--gdb', '-g', help='Path to GDB executable')
parser.add_argument('--elf-dir', '-e', action='append', help='Additional directories to search for ELF files')
parser.add_argument('--ps-profile', default=PS_PROFILE_PATH,
                    help='Path to ESP-IDF PowerShell profile (default: %(default)s)')
parser.add_argument('--skip-esp-setup', action='store_true',
                    help='Skip ESP-IDF environment setup')
parser.add_argument('core-dump-file', help='Path to core dump file')

args = parser.parse_args()
kwargs = {k: v for k, v in vars(args).items() if v is not None}
kwargs["core"] = kwargs.pop('core-dump-file')

# ====== 1. ПОВТОРНАЯ ИНИЦИАЛИЗАЦИЯ ESP-IDF (если указан другой профиль) ======
if not kwargs.get('skip_esp_setup'):
    ps_profile_path = kwargs.get('ps_profile')

    # Если пользователь указал другой профиль или нам не удалось инициализироваться ранее
    if ps_profile_path != PS_PROFILE_PATH or 'PATH' not in os.environ or 'Espressif' not in os.environ.get('PATH', ''):
        if ps_profile_path and os.path.exists(ps_profile_path):
            print(f"\nИспользование пользовательского профиля: {ps_profile_path}")
            setup_esp_idf_environment(ps_profile_path)
        elif not os.path.exists(PS_PROFILE_PATH):
            print(f"⚠ ВНИМАНИЕ: Файл профиля не найден: {PS_PROFILE_PATH}")
            print("  Попробую найти GDB без настройки окружения...")

# ====== 2. ПОИСК GDB ======
if 'gdb' not in kwargs or not kwargs['gdb']:
    print("\nАвтоматический поиск GDB...")

    # Сначала проверяем стандартные пути ESP-IDF
    espressif_paths = [
        "C:\\Espressif\\tools\\xtensa-esp32s3-elf\\esp-*\\xtensa-esp32s3-elf\\bin\\xtensa-esp32s3-elf-gdb.exe",
        "C:\\Espressif\\tools\\riscv32-esp-elf\\esp-*\\riscv32-esp-elf\\bin\\riscv32-esp-elf-gdb.exe",
        "C:\\Espressif\\frameworks\\esp-idf-v*\\tools\\**\\*gdb*.exe"
    ]

    for pattern in espressif_paths:
        try:
            files = glob.glob(pattern, recursive=True)
            for file in files:
                if os.path.isfile(file):
                    kwargs['gdb'] = file
                    print(f"Найден GDB в ESP-IDF: {file}")
                    break
            if 'gdb' in kwargs:
                break
        except:
            continue

    # Если не нашли, ищем в системе
    if 'gdb' not in kwargs:
        found_gdb = find_gdb_in_esp_idf()
        if found_gdb:
            kwargs['gdb'] = found_gdb

    # Если всё ещё не нашли, пытаемся найти в PATH
    if 'gdb' not in kwargs:
        gdb_names = ['xtensa-esp32s3-elf-gdb', 'riscv32-esp-elf-gdb', 'gdb']
        for gdb_name in gdb_names:
            try:
                result = subprocess.run(['where', gdb_name], capture_output=True, text=True)
                if result.returncode == 0 and result.stdout.strip():
                    kwargs['gdb'] = result.stdout.strip().split('\n')[0]
                    print(f"Найден GDB в PATH: {kwargs['gdb']}")
                    break
            except:
                continue

# Проверяем, что GDB существует
if 'gdb' in kwargs and kwargs['gdb']:
    if not os.path.exists(kwargs['gdb']):
        print(f"ОШИБКА: GDB не найден по пути: {kwargs['gdb']}")

        # Предлагаем варианты решения
        print("\nПожалуйста, укажите путь к GDB одним из способов:")
        print("1. Установите ESP-IDF toolchain")
        print("2. Укажите путь к GDB явно: --gdb \"C:\\Espressif\\tools\\...\\xtensa-esp32s3-elf-gdb.exe\"")
        print("3. Установите отдельно GDB для Windows")
        sys.exit(1)
    print(f"Используется GDB: {kwargs['gdb']}")
else:
    print("ВНИМАНИЕ: GDB не найден!")
    print("Анализ может завершиться ошибкой.")

# ====== 3. ОБРАБАТЫВАЕМ CORE DUMP ======
print(f"\nЗагрузка core dump: {kwargs['core']}")
loader = ESPCoreDumpFileLoader(kwargs["core"])
loader._load_core_src()

loader.core_elf_file = loader._create_temp_file()
with open(loader.core_elf_file, 'wb') as fw:
    fw.write(loader.core_src.data)

core_elf = ESPCoreDumpElfFile(loader.core_elf_file, e_machine=ESPCoreDumpElfFile.EM_XTENSA)

core_sha_trimmed = None
for seg in core_elf.note_segments:
    for note_sec in seg.note_secs:
        if note_sec.name == b'ESP_CORE_DUMP_INFO' and note_sec.type == ESPCoreDumpElfFile.PT_ESP_INFO:
            coredump_sha256_struct = Struct('ver' / Int32ul, 'sha256' / Bytes(64))
            coredump_sha256 = coredump_sha256_struct.parse(note_sec.desc[:coredump_sha256_struct.sizeof()])
            core_sha_trimmed = coredump_sha256.sha256.rstrip(b'\x00').decode()
            print(f'Core dump SHA256: {core_sha_trimmed}')

# ====== 4. ПОИСК ELF ФАЙЛА ======
if 'prog' not in kwargs or not kwargs['prog']:
    search_dirs = ['.', 'build', 'build/elf', 'build/elf/esp32-s3-devkitc-1-16m', '../build', '../../build']

    if 'elf_dir' in kwargs:
        if isinstance(kwargs['elf_dir'], list):
            search_dirs.extend(kwargs['elf_dir'])
        else:
            search_dirs.append(kwargs['elf_dir'])

    # Ищем ELF файлы по частичному хэшу
    if core_sha_trimmed:
        print(f"\nПоиск ELF файла с частичным хэшем: {core_sha_trimmed}")
        matching_files = find_elf_by_partial_hash(core_sha_trimmed, search_dirs)

        if matching_files:
            if len(matching_files) == 1:
                kwargs["prog"] = matching_files[0]
                print(f"Найден ELF файл: {matching_files[0]}")
            else:
                print(f"Найдено несколько подходящих ELF файлов:")
                for i, elf_file in enumerate(matching_files):
                    print(f"  {i+1}. {elf_file}")

                # Выбираем первый (самый вероятный)
                kwargs["prog"] = matching_files[0]
                print(f"Используется: {matching_files[0]}")
        else:
            # Если не нашли по хэшу, ищем стандартные имена
            print("\nНе найдено ELF файлов с указанным хэшем, ищем стандартные имена...")

            standard_elf_names = [
                "ssvc_open_connect.elf",
                "firmware.elf",
                "main.elf",
                "app.elf"
            ]

            # Проверяем стандартные имена во всех директориях
            for search_dir in search_dirs:
                if os.path.isdir(search_dir):
                    for elf_name in standard_elf_names:
                        elf_path = os.path.join(search_dir, elf_name)
                        if os.path.exists(elf_path):
                            kwargs["prog"] = elf_path
                            print(f"Найден стандартный ELF: {elf_path}")
                            break
                    if 'prog' in kwargs:
                        break

            if 'prog' not in kwargs:
                # Последняя попытка: ищем любой ELF файл
                print("\nПоиск любого ELF файла в проекте...")
                all_elf_files = []
                for search_dir in search_dirs:
                    if os.path.isdir(search_dir):
                        pattern = os.path.join(search_dir, "**", "*.elf")
                        elf_files = glob.glob(pattern, recursive=True)
                        all_elf_files.extend(elf_files)

                if all_elf_files:
                    # Сортируем по дате изменения (новые первыми)
                    all_elf_files.sort(key=lambda x: os.path.getmtime(x) if os.path.exists(x) else 0, reverse=True)

                    print(f"Найдено ELF файлов: {len(all_elf_files)}")
                    print("Последние 5 файлов:")
                    for i, elf_file in enumerate(all_elf_files[:5]):
                        mtime = os.path.getmtime(elf_file)
                        from datetime import datetime
                        mtime_str = datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')
                        print(f"  {i+1}. {os.path.basename(elf_file)} - {mtime_str}")

                    kwargs["prog"] = all_elf_files[0]
                    print(f"\nИспользуется последний ELF файл: {all_elf_files[0]}")
                else:
                    print("\nERROR: Не найден ни один ELF файл!")
                    print("Пожалуйста, укажите ELF файл явно:")
                    print("  python analyze_dump.py --prog путь/к/файлу.elf coredump.bin")
                    print("\nИли укажите директории для поиска:")
                    print("  python analyze_dump.py --elf-dir build --elf-dir ../build coredump.bin")
                    sys.exit(1)

print(f"\nИспользуется ELF файл: {kwargs['prog']}")

# ====== 5. ЗАПУСК АНАЛИЗА ======
print("\nЗапуск анализа core dump...")
print("=" * 60)

# Удаляем неиспользуемые аргументы
for key in ['elf_dir', 'ps_profile', 'skip_esp_setup']:
    if key in kwargs:
        del kwargs[key]

try:
    espcoredump = CoreDump(**kwargs)
    temp_core_files = espcoredump.info_corefile()

    if temp_core_files:
        for f in temp_core_files:
            try:
                os.remove(f)
            except OSError:
                pass

    print("\n✓ Анализ завершен успешно!")

except Exception as e:
    print(f"\n✗ Ошибка при анализе core dump: {e}")

    # Проверяем, связана ли ошибка с GDB
    if "GDB executable not found" in str(e):
        print("\nПроблема с GDB. Попробуйте следующие решения:")
        print("1. Укажите путь к GDB явно:")
        print("   python analyze_dump.py --gdb \"C:\\Espressif\\tools\\...\\xtensa-esp32s3-elf-gdb.exe\" coredump.bin")
        print("\n2. Проверьте установку ESP-IDF:")
        print("   Убедитесь, что файл существует: C:\\Espressif\\tools\\Microsoft.v5.5.2.PowerShell_profile.ps1")
        print("\n3. Установите toolchain вручную:")
        print("   Запустите: C:\\Espressif\\tools\\idf-env.exe")
        print("   Или установите через ESP-IDF Tools Installer")

    print(f"\nПопробуйте указать ELF файл явно:")
    print(f"python analyze_dump.py --prog build\\ssvc_open_connect.elf coredump.bin")

    sys.exit(1)
