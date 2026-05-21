# Release — Android TV pre-release

## Build release APK (CI or local)

```bash
./gradlew :app:assembleRelease
```

Output: `app/build/outputs/apk/release/app-release.apk`

Release builds use the **debug keystore** for sideload pre-releases (no Play Store signing).

## Download from GitHub

Pre-release APK: [v0.1.2-pre](https://github.com/macdirtycow/OmiibaConnect-AndroidTV/releases/tag/v0.1.2-pre) (`app-release.apk`).

CI builds on every push to `main` and refreshes that asset.

## Install on Google / Android TV

1. Pair your Sony headphones in **Settings → Remotes & accessories → Bluetooth** (or system Bluetooth).
2. Install the APK:
   - **ADB** (developer options + network debugging):  
     `adb connect <tv-ip>` then  
     `adb install -r app-release.apk`
   - Or use a file manager / “Send files to TV” and open the APK (if your TV allows unknown sources).

3. Open **Omiiba Connect** from the TV app row. Run **RFCOMM spike** first if connect fails (see [android-tv-bluetooth.md](android-tv-bluetooth.md)).

## Manual GitHub release (maintainers)

```bash
./gradlew :app:assembleRelease
gh release create v0.1.0-pre app/build/outputs/apk/release/app-release.apk \
  --title "Omiiba Connect TV 0.1.0 (pre-release)" \
  --prerelease \
  --notes "Release APK for Android TV sideload."
```
