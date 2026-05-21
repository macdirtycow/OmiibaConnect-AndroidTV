#!/usr/bin/env bash
# Copy portable protocol sources from OmiibaConnect macOS repo.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MAC_ROOT="${MAC_ROOT:-$(cd "$ROOT/../SonyXM3Mac" && pwd)}"
DEST="$ROOT/core-native/src/main/cpp"
FILES=(
  ByteMagic.cpp ByteMagic.h
  BluetoothWrapper.cpp BluetoothWrapper.h
  CommandSerializer.cpp CommandSerializer.h
  Constants.h DeviceProfile.h DeviceStatus.h
  Exceptions.h Headphones.cpp Headphones.h
  IBluetoothConnector.h ProtocolParser.cpp ProtocolParser.h
)
for f in "${FILES[@]}"; do
  cp "$MAC_ROOT/Client/$f" "$DEST/$f"
done
# Android-specific files must stay:
# AndroidBluetoothConnector.* omiiba_jni.cpp CMakeLists.txt
sed -i '' '/SingleInstanceFuture/d' "$DEST/Headphones.h" 2>/dev/null || sed -i '/SingleInstanceFuture/d' "$DEST/Headphones.h"
echo "Synced protocol from $MAC_ROOT to $DEST"
