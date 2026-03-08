# Скрипты проекта

Скрипты разделены по назначению:

| Каталог | Назначение | Примеры |
|---------|------------|---------|
| **pio/** | PlatformIO — pre/post build (вызываются из `platformio.ini`) | `build_interface.py`, `generate_cert_bundle.py`, `merge_bin.py`, `rename_fw.py`, `save_elf.py` |
| **docker/** | Сборка и тесты в Docker | `docker_build.ps1`, `docker_test_native.ps1`, `docker_image_build.ps1` |
| **test/** | Локальный запуск тестов (native, embedded) | `run_native_tests.ps1`, `run_all_tests.ps1` |
| **docs/** | Сборка документации (MkDocs) | `docs_build_all.ps1` |

## Использование

```powershell
# Docker
.\scripts\docker\docker_build.ps1
.\scripts\docker\docker_test_native.ps1
.\scripts\docker\docker_image_build.ps1

# Тесты
.\scripts\test\run_native_tests.ps1
.\scripts\test\run_all_tests.ps1 -Port COM25

# Документация
.\scripts\docs\docs_build_all.ps1
```
