#!/usr/bin/env python3
"""
Копирует webflash.bin в docs/firmware, генерирует JSON-манифесты и manifest_index.json.
Источники: локальная сборка, артефакты CI или релизы GitHub.

Использование:
  Локально (сборка + копирование): python scripts/docs/update_firmware_docs.py
  Из build/: python scripts/docs/update_firmware_docs.py build
  Из артефактов CI: python scripts/docs/update_firmware_docs.py all-artifacts 0-2-6-1
  С GitHub: python scripts/docs/update_firmware_docs.py --github
  Только manifest_index: python scripts/docs/update_firmware_docs.py --manifest-only
"""
import json
import re
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_DIR = PROJECT_ROOT / "docs" / "firmware"
BASE_PATH = "/ssvc_open_connect/firmware"
GITHUB_REPO = "SSVC0059/ssvc_open_connect"
META_FILE = FIRMWARE_DIR / "release_meta.json"

VARIANT_LABELS = {
    "16m": "16MB Flash",
    "8m": "8MB Flash",
    "16m-secure": "16MB Flash (Secure)",
    "8m-secure": "8MB Flash (Secure)",
}

FIRMWARE_ENVS = [
    "esp32-s3-devkitc-1-8m",
    "esp32-s3-devkitc-1-8m-secure",
    "esp32-s3-devkitc-1-16m",
    "esp32-s3-devkitc-1-16m-secure",
]

MANIFEST_TEMPLATE = {
    "name": "SSVC Open Connect",
    "home_assistant_domain": "ssvc_open_connect",
    "new_install_prompt_erase": True,
    "builds": [
        {
            "chipFamily": "ESP32-S3",
            "parts": [{"path": "", "offset": 0}],
        }
    ],
}


def extract_version_from_filename(name: str) -> str | None:
    """Извлекает 0-2-6-1 из SSVC-Open-Connect_esp32-s3-devkitc-1-16m_0-2-6-1_webflash.bin"""
    m = re.search(r"_(\d+(?:-\d+)*)_webflash\.bin$", name)
    return m.group(1) if m else None


def run_platformio_build() -> bool:
    """Запускает сборку по всем целям PlatformIO. Возвращает True при успехе."""
    envs = " ".join(f"-e {e}" for e in FIRMWARE_ENVS)
    cmd = f"pio run {envs}"
    print(f"Сборка: {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=PROJECT_ROOT)
    return result.returncode == 0


def download_from_github() -> list[tuple[Path, str]]:
    """
    Скачивает _webflash.bin из релизов GitHub.
    Возвращает список (путь_к_файлу, version_dot).
    """
    url = f"https://api.github.com/repos/{GITHUB_REPO}/releases"
    req = urllib.request.Request(url, headers={"Accept": "application/vnd.github.v3+json"})
    with urllib.request.urlopen(req) as r:
        releases = json.loads(r.read().decode())

    FIRMWARE_PATTERNS = ["16m_", "8m_", "16m-secure_", "8m-secure_"]

    def get_webflash_assets(release):
        return [
            a
            for a in release.get("assets", [])
            if a["name"].endswith("_webflash.bin")
            and any(p in a["name"] for p in FIRMWARE_PATTERNS)
        ]

    def parse_ver(tag):
        m = re.search(r"v?(\d+(?:\.\d+)*)", tag or "")
        return tuple(int(x) for x in m.group(1).split(".")) if m else ()

    releases_with_webflash = [
        r for r in releases
        if get_webflash_assets(r)
    ]
    if not releases_with_webflash:
        raise ValueError("Нет релизов с _webflash.bin")

    # Стабильная = последний релиз с prerelease=false, тестовая = последний prerelease
    non_prerelease = [r for r in releases_with_webflash if not r.get("prerelease", False)]
    prereleases = [r for r in releases_with_webflash if r.get("prerelease", False)]

    stable_release = max(non_prerelease, key=lambda r: parse_ver(r["tag_name"])) if non_prerelease else None
    test_release = max(prereleases, key=lambda r: parse_ver(r["tag_name"])) if prereleases else None

    downloaded = []
    FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

    for release in [stable_release, test_release]:
        if not release:
            continue
        tag = release["tag_name"]
        version_dash = re.sub(r"^v", "", tag).replace(".", "-")
        version_dot = tag.lstrip("v")
        assets = get_webflash_assets(release)
        if not assets:
            print(f"Пропуск {tag}: нет _webflash.bin")
            continue
        print(f"Загрузка {tag} ({len(assets)} файлов)...")
        for asset in assets:
            dest = FIRMWARE_DIR / asset["name"]
            try:
                urllib.request.urlretrieve(asset["browser_download_url"], dest)
                json_name = asset["name"].replace(".bin", ".json")
                manifest = dict(MANIFEST_TEMPLATE)
                manifest["version"] = version_dot
                manifest["builds"][0]["parts"][0]["path"] = f"{BASE_PATH}/{asset['name']}"
                (FIRMWARE_DIR / json_name).write_text(
                    json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8"
                )
                downloaded.append((dest, version_dot))
                print(f"  {asset['name']}")
            except Exception as e:
                print(f"  Ошибка {asset['name']}: {e}")

    # Метаданные для manifest_index: стабильная/тестовая из GitHub
    meta = {"source": "github", "stable": None, "test": None}
    if stable_release:
        meta["stable"] = stable_release["tag_name"].lstrip("v")
    if test_release:
        meta["test"] = test_release["tag_name"].lstrip("v")
    (FIRMWARE_DIR / "release_meta.json").write_text(
        json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8"
    )

    return downloaded


def version_to_sort(s: str) -> tuple:
    """'0.2.6.1' -> (0, 2, 6, 1)"""
    return tuple(int(x) for x in s.replace("-", ".").split("."))


def parse_manifest_filename(name: str) -> dict | None:
    """Парсит имя файла SSVC-Open-Connect_esp32-s3-devkitc-1-16m_0-2-5-2_webflash.json"""
    if not name.endswith("_webflash.json"):
        return None
    m = re.search(r"-(16m|8m)(?:-secure)?_(\d+(?:-\d+)*)_webflash\.json$", name)
    if not m:
        return None
    variant = m.group(1) + ("-secure" if "-secure" in name else "")
    version_dash = m.group(2)
    version = version_dash.replace("-", ".")
    return {
        "variant": variant,
        "version": version,
        "version_sort": tuple(int(x) for x in version_dash.split("-")),
        "manifestUrl": f"{BASE_PATH}/{name}",
        "label": VARIANT_LABELS.get(variant, variant.replace("-", " ").upper() + " Flash"),
    }


def generate_manifest_index() -> int:
    """Генерирует manifest_index.json по содержимому docs/firmware/."""
    if not FIRMWARE_DIR.exists():
        print(f"Директория не найдена: {FIRMWARE_DIR}")
        return 1

    manifests = []
    for f in FIRMWARE_DIR.glob("*_webflash.json"):
        data = parse_manifest_filename(f.name)
        if data:
            manifests.append(data)

    if not manifests:
        print("Не найдено подходящих *_webflash.json файлов")
        return 1

    def sort_key(v):
        url = v["manifestUrl"]
        if "16m-secure" in url:
            return (1, v["label"])
        if "16m" in url:
            return (0, v["label"])
        if "8m-secure" in url:
            return (3, v["label"])
        return (2, v["label"])

    def make_entry(m):
        return {"manifestUrl": m["manifestUrl"], "label": m["label"], "version": m["version"]}

    meta = None
    if META_FILE.exists():
        try:
            meta = json.loads(META_FILE.read_text(encoding="utf-8"))
        except Exception:
            pass

    stable_variants = []
    test_variants = []
    local_variants = []

    if meta and meta.get("source") == "github" and meta.get("stable"):
        stable_ver = version_to_sort(meta["stable"])
        test_ver = version_to_sort(meta["test"]) if meta.get("test") else None
        for m in manifests:
            vs = m["version_sort"]
            if vs == stable_ver:
                stable_variants.append({**make_entry(m), "isStable": True})
            elif test_ver is not None and vs == test_ver:
                test_variants.append({**make_entry(m), "isStable": False})
            elif test_ver is not None and vs > test_ver:
                local_variants.append({**make_entry(m), "isLocal": True})
        for lst in [stable_variants, test_variants, local_variants]:
            lst.sort(key=sort_key)
    else:
        all_versions = sorted(set(m["version_sort"] for m in manifests), reverse=True)
        test_version = all_versions[0]
        stable_version = all_versions[1] if len(all_versions) > 1 else all_versions[0]
        stable_variants = [
            {**make_entry(m), "isStable": True}
            for m in manifests if m["version_sort"] == stable_version
        ]
        test_variants = [
            {**make_entry(m), "isStable": False}
            for m in manifests
            if m["version_sort"] == test_version and test_version != stable_version
        ]
        for lst in [stable_variants, test_variants]:
            lst.sort(key=sort_key)

    output = {
        "source": meta.get("source", "local") if meta else "local",
        "stable": stable_variants,
        "test": test_variants,
    }
    if local_variants:
        output["local"] = local_variants

    output_file = FIRMWARE_DIR / "manifest_index.json"
    output_file.write_text(json.dumps(output, indent=2, ensure_ascii=False), encoding="utf-8")
    parts = []
    if stable_variants:
        parts.append(f"стабильная {stable_variants[0]['version']} ({len(stable_variants)})")
    if test_variants:
        parts.append(f"тестовая {test_variants[0]['version']} ({len(test_variants)})")
    if local_variants:
        parts.append(f"локальная {local_variants[0]['version']} ({len(local_variants)})")
    print(f"Создан {output_file}: {', '.join(parts)}")
    return 0


def process_bin_files(bin_files: list[Path], version_dash: str | None) -> None:
    """Копирует bin-файлы в docs/firmware и создаёт JSON манифесты."""
    FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)
    for bin_file in bin_files:
        file_version = extract_version_from_filename(bin_file.name)
        version_dot = (version_dash or file_version or "").replace("-", ".")
        if not version_dot:
            print(f"Пропуск {bin_file.name}: не удалось определить версию")
            continue
        dest_bin = FIRMWARE_DIR / bin_file.name
        if bin_file != dest_bin:
            shutil.copy2(bin_file, dest_bin)
        json_name = bin_file.name.replace(".bin", ".json")
        dest_json = FIRMWARE_DIR / json_name
        manifest = dict(MANIFEST_TEMPLATE)
        manifest["version"] = version_dot
        manifest["builds"][0]["parts"][0]["path"] = f"{BASE_PATH}/{bin_file.name}"
        dest_json.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"Добавлено: {bin_file.name} и {json_name}")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--manifest-only" in sys.argv:
        return generate_manifest_index()
    if "--github" in sys.argv:
        print("Загрузка релизов с GitHub...")
        try:
            download_from_github()
        except Exception as e:
            print(f"Ошибка: {e}")
            return 1
    else:
        artifacts_dir = Path(args[0]).resolve() if args else PROJECT_ROOT / "build"
        version_dash = args[1] if len(args) > 1 else None

        if len(args) == 0:
            if not run_platformio_build():
                print("Ошибка сборки")
                return 1

        if not artifacts_dir.exists():
            print(f"Директория не найдена: {artifacts_dir}")
            print("Укажите путь к build/ или all-artifacts/, либо --github для загрузки с GitHub")
            return 1

        bin_files = []
        for variant in ["16m_", "8m_", "16m-secure_", "8m-secure_"]:
            pattern = f"*{variant}*_webflash.bin"
            bin_files.extend(artifacts_dir.rglob(pattern))

        if not bin_files:
            print(f"Не найдено *_webflash.bin в {artifacts_dir}")
            return 1

        process_bin_files(bin_files, version_dash)

    return generate_manifest_index()


if __name__ == "__main__":
    sys.exit(main())
