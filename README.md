# Omiiba Connect for Android TV

> **Gearchiveerd (mei 2026)** — Op de meeste Google TV / Philips-tv’s opent **RFCOMM niet** naar WH-1000XM3 (alleen TV-audio/A2DP werkt). Sony-bediening via deze app is op die hardware niet haalbaar. Gebruik **[Omiiba Connect voor macOS](https://github.com/macdirtycow/OmiibaConnect)** of Sony Sound Connect op je telefoon.

Unofficial **Android TV** companion for Sony WH-1000XM3 / XM4 / XM5 / XM6. Uses the same reverse-engineered MDR protocol as [OmiibaConnect for macOS](https://github.com/macdirtycow/OmiibaConnect).

**Not affiliated with Sony.**

## Laatste release

Pre-release APK’s blijven op [GitHub Releases](https://github.com/macdirtycow/OmiibaConnect-AndroidTV/releases) (tot **v0.1.15-pre**).

## Waarom gearchiveerd

| Werkt | Werkt niet op typische TV |
|-------|---------------------------|
| Diagnose, permissies, UI | RFCOMM naar XM3 (`socket gesloten`) |
| TV Bluetooth-audio (A2DP) | Zelfde bediening als Sound Connect |

Zie [docs/android-tv-sony-apk.md](docs/android-tv-sony-apk.md) — Sony telefoon-APK gebruikt voor XM3 ook **SPP/RFCOMM** (`96CC203E-…`), niet LE-GATT.

## Broncode

Historische documentatie: [docs/android-tv-bluetooth.md](docs/android-tv-bluetooth.md).
