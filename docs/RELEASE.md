# Release — Android TV pre-release

## Build APK

```bash
gradle wrapper   # first time only (needs gradle-wrapper.jar)
./gradlew :app:assembleDebug
```

Output: `app/build/outputs/apk/debug/app-debug.apk`

## GitHub pre-release

```bash
git tag -a v0.1.0-pre -m "Omiiba Connect Android TV pre-release"
git push origin main --tags
gh release create v0.1.0-pre \
  app/build/outputs/apk/debug/app-debug.apk \
  --title "Omiiba Connect TV 0.1.0 (pre-release)" \
  --prerelease \
  --notes "Pre-release. Requires RFCOMM spike pass on your TV. See docs/android-tv-bluetooth.md"
```

## Publish repository

```bash
gh repo create OmiibaConnect-AndroidTV --public --source=. --remote=origin --push
```

(Use your preferred org/name; not affiliated with Sony.)
