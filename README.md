# Omiiba Connect for Android TV

Unofficial **Android TV** companion for Sony WH-1000XM3 / XM4 / XM5 / XM6. Uses the same reverse-engineered MDR protocol as [OmiibaConnect for macOS](https://github.com/macdirtycow/OmiibaConnect).

**Not affiliated with Sony.**

## Status

Pre-release (`0.1.1-pre`). Requires a TV that supports **Bluetooth Classic RFCOMM** to the headphones. See [docs/android-tv-bluetooth.md](docs/android-tv-bluetooth.md).

## Features

- Connect to paired Sony headphones
- Ambient sound / ASM level / focus on voice
- Virtual sound (surround + sound position)
- Equalizer presets
- Touch sensor & voice guidance (model-dependent)
- Battery, codec, firmware readout
- Background keepalive + `prepareForControl` (Mac 2.5.x behaviour)

## Download

Pre-release **release APK** (sideload): [GitHub Releases v0.1.1-pre](https://github.com/macdirtycow/OmiibaConnect-AndroidTV/releases/tag/v0.1.1-pre).

## Build

```bash
cd omiiba-connect-android-tv
./gradlew :app:assembleRelease
```

APK: `app/build/outputs/apk/release/app-release.apk`

Requires Android SDK 34 and NDK (CMake builds `core-native`).

## Install on TV

```bash
adb install -r app/build/outputs/apk/release/app-release.apk
```

Pair headphones in system Bluetooth settings first.

## RFCOMM spike (phase 0)

```bash
adb shell am start -a dev.omiiba.connect.tv.RFCOMM_SPIKE -n dev.omiiba.connect.tv/.RfcommSpikeActivity
```

## Protocol sync

Portable C++ is copied from the Mac repo:

```bash
MAC_ROOT=../SonyXM3Mac ./scripts/sync-protocol-from-mac.sh
```

## Docs

- [Bluetooth feasibility](docs/android-tv-bluetooth.md)
- [Regression checklist](docs/regression-checklist-android-tv.md)
- Mac [APK reference](https://github.com/macdirtycow/OmiibaConnect/blob/master/docs/apk-reference.md)

## License

MIT — see [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).
