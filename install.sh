#!/bin/bash
# Installs the newest ViolinMIDI into the Programs folder. Run it with:
#
#   curl -fsSL https://raw.githubusercontent.com/XLeathnex/ViolinMIDI/main/install.sh | bash
#
# It only uses tools that are part of macOS. Files loaded in the Terminal are not
# marked as "from the internet", so macOS does not block the app afterwards.
set -euo pipefail

URL="https://github.com/XLeathnex/ViolinMIDI/releases/latest/download/ViolinMIDI-macOS.zip"
NAME="ViolinMIDI.app"

fail() {
    echo
    echo "Fehler: $1"
    exit 1
}

[ "$(uname -s)" = "Darwin" ] || fail "Diese Installation ist nur für den Mac."

# The Programs folder if it can be written to, otherwise the user's own one.
TARGET_DIR="/Applications"
if [ ! -w "$TARGET_DIR" ]; then
    TARGET_DIR="$HOME/Applications"
    mkdir -p "$TARGET_DIR"
fi
APP="$TARGET_DIR/$NAME"

if pgrep -x ViolinMIDI >/dev/null 2>&1; then
    fail "ViolinMIDI läuft gerade. Bitte die App beenden und die Zeile noch einmal ausführen."
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "ViolinMIDI wird geladen ..."
curl -fL --progress-bar -o "$WORK/ViolinMIDI.zip" "$URL" \
    || fail "Der Download hat nicht geklappt. Besteht eine Internetverbindung?"

echo "Die App wird entpackt und geprüft ..."
ditto -x -k "$WORK/ViolinMIDI.zip" "$WORK/unpacked" || fail "Das Archiv ließ sich nicht entpacken."
NEW="$WORK/unpacked/$NAME"
[ -x "$NEW/Contents/MacOS/ViolinMIDI" ] || fail "Das Archiv enthält keine ViolinMIDI-App."
codesign --verify --deep --strict "$NEW" 2>/dev/null || fail "Die geladene App ist beschädigt."
xattr -dr com.apple.quarantine "$NEW" 2>/dev/null || true
VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$NEW/Contents/Info.plist" 2>/dev/null || echo "?")"

if [ -e "$APP" ]; then
    echo "Die vorhandene App wird ersetzt ..."
    rm -rf "$APP" 2>/dev/null || true
    if [ -e "$APP" ]; then
        fail "macOS lässt das Ersetzen vom Terminal aus nicht zu. Bitte die alte ViolinMIDI-App im Ordner Programme in den Papierkorb legen und die Zeile noch einmal ausführen."
    fi
fi

ditto "$NEW" "$APP" || fail "Die App ließ sich nicht nach $TARGET_DIR kopieren."

echo
echo "Fertig. ViolinMIDI $VERSION liegt in $TARGET_DIR."
echo "Beim ersten Start fragt macOS nach dem Mikrofon – bitte erlauben."

# VIOLINMIDI_NO_OPEN=1 installs without starting the app (used by the automatic test).
if [ -z "${VIOLINMIDI_NO_OPEN:-}" ]; then
    open "$APP"
fi
