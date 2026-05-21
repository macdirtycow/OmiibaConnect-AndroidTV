# Android TV — app starten (officiële eisen)

Gebaseerd op [Create and run a TV app](https://developer.android.com/training/tv/get-started/create) en [Leanback browse](https://developer.android.com/training/tv/playback/leanback/browse).

## Vereisten in de app

| Eis | Omiiba Connect |
|-----|----------------|
| `MAIN` + `LEANBACK_LAUNCHER` | Ja (beide categorieën op `MainActivity`) |
| `android.software.leanback` | Ja (`required=false` voor bredere sideload) |
| Touchscreen niet verplicht | Ja |
| `android:banner` (320×180) | Ja |
| `android:icon` | Ja |
| Leanback-thema op TV-activity | `Theme.Leanback.Browse` |
| `BrowseSupportFragment` in layout-XML | Ja (`FragmentContainerView`) |
| Native/BT pas bij gebruik | Ja (geen load bij cold start) |

## Installeren / starten op Google TV

```bash
adb connect <tv-ip>
adb install -r app-release.apk
adb shell am start -n dev.omiiba.connect.tv/.MainActivity
```

Als de app in het menu niet opent, controleer crash-log:

```bash
adb logcat -c
adb shell am start -n dev.omiiba.connect.tv/.MainActivity
adb logcat -d | grep -E "AndroidRuntime|FATAL|omiiba|Omiiba"
```

## Veelvoorkomende oorzaken “opent niet”

1. **Crash bij start** — zie `FATAL EXCEPTION` in logcat (niet de Gradle compile-warnings).
2. **Alleen LEANBACK_LAUNCHER** — sommige sideload-launchers verwachten ook `LAUNCHER` (nu toegevoegd).
3. **Fragment via `commit()` i.p.v. XML** — Leanback-sample gebruikt `<fragment>` / `FragmentContainerView` (nu toegevoegd).
4. **Native library bij open** — `System.loadLibrary` gebeurt pas bij eerste “Connect” (v0.1.5+).
