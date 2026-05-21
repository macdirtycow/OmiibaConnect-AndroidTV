#!/bin/sh
# Gradle start script (minimal wrapper — run `gradle wrapper` locally if this fails).
DIR="$(cd "$(dirname "$0")" && pwd)"
GRADLE_USER_HOME="${GRADLE_USER_HOME:-$HOME/.gradle}"
WRAPPER_JAR="$DIR/gradle/wrapper/gradle-wrapper.jar"
if [ ! -f "$WRAPPER_JAR" ]; then
  echo "Missing gradle-wrapper.jar. Install Gradle and run: gradle wrapper" >&2
  exit 1
fi
exec java -jar "$WRAPPER_JAR" "$@"
