#!/usr/bin/env bash
# Run from your Mac when the TV is reachable via adb connect <ip>
set -euo pipefail

PKG="dev.omiiba.connect.tv"
DIAG=".TvDiagnosticsActivity"
MAIN=".MainActivity"
APK="${1:-app/build/outputs/apk/release/app-release.apk}"

if [[ -n "${TV_IP:-}" ]]; then
  echo "Connecting adb to ${TV_IP}…"
  adb connect "${TV_IP}" || true
fi

if [[ -f "$APK" ]]; then
  echo "Installing ${APK}…"
  adb install -r "$APK"
else
  echo "No APK at ${APK} — skip install (pass path as first argument)."
fi

echo ""
echo "=== Start diagnose-scherm (tekst op TV) ==="
adb shell am start -n "${PKG}/${DIAG}" || true
sleep 3

echo ""
echo "=== Laatste crash-bestand op de TV (via adb) ==="
adb exec-out run-as "${PKG}" cat files/last_crash.txt 2>/dev/null || echo "(geen crash-bestand of run-as niet beschikbaar)"

echo ""
echo "=== Logcat (Omiiba + crashes) ==="
adb logcat -d -s OmiibaCrash AndroidRuntime ActivityManager 2>/dev/null | tail -80 || adb logcat -d | tail -80

echo ""
echo "=== Optioneel: hoofdapp starten ==="
echo "adb shell am start -n ${PKG}/${MAIN}"
