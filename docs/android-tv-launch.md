# Android TV — starten met alleen ADB

Je hebt geen logcat-viewer nodig op de TV zelf. Vanaf je Mac met `adb connect <tv-ip>` kun je alles uitlezen.

## 1. Diagnose-scherm op de TV (geen gissen)

Vanaf **v0.1.6-pre** opent de app eerst een **gewoon tekstscherm** (geen Leanback):

- Versie, Android-API, CPU-ABI
- Test native bibliotheek (`omiiba_core`)
- Bluetooth-rechten
- Laatste crash (als die er was)

**Maak een foto** van dat scherm als iets **MISLUKT** staat — dat is genoeg om te fixen.

Daarna: knop **“Open Omiiba Connect (OK)”** → hoofdapp met Leanback-menu.

## 2. Script op je Mac

```bash
cd omiiba-connect-android-tv
export TV_IP=192.168.1.50   # IP van je TV
chmod +x scripts/adb-tv-diagnose.sh
./scripts/adb-tv-diagnose.sh
```

Met APK-pad:

```bash
./scripts/adb-tv-diagnose.sh app/build/outputs/apk/release/app-release.apk
```

## 3. Handmatige adb-commando’s

```bash
adb connect <tv-ip>
adb install -r app-release.apk

# Diagnose (start altijd dit eerst)
adb shell am start -n dev.omiiba.connect.tv/.TvDiagnosticsActivity

# Crash-bestand ophalen (debug-build)
adb exec-out run-as dev.omiiba.connect.tv cat files/last_crash.txt

# Logcat naar bestand op je Mac
adb logcat -c
adb shell am start -n dev.omiiba.connect.tv/.TvDiagnosticsActivity
sleep 3
adb logcat -d > tv-log.txt
grep -E "FATAL|OmiibaCrash|AndroidRuntime" tv-log.txt
```

## 4. Officiële TV-eisen (samenvatting)

Zie [Create and run a TV app](https://developer.android.com/training/tv/get-started/create):

- `LEANBACK_LAUNCHER` + `LAUNCHER`
- `android:banner` + icoon
- Leanback-thema voor het hoofdscherm
- `BrowseSupportFragment` in layout-XML

Diagnose-activity gebruikt bewust **geen** Leanback, zodat je ziet of het probleem in het hoofdmenu zit.
