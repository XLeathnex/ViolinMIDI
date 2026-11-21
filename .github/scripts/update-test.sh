#!/bin/bash
# Exercises the updater on a real Mac, without the network:
# install a second version next to the app, switch to it, switch back,
# and make sure a version that cannot start does not lock the user out.
#
# usage: update-test.sh path/to/ViolinMIDI.app
set -euo pipefail

APP="$1"
T="${RUNNER_TEMP:-/tmp}/violinmidi-update-test"
VERSIONS="$HOME/Library/Application Support/ViolinMIDI/Versions"

rm -rf "$T" "$VERSIONS"
mkdir -p "$T/inst" "$T/new"

# The "installed" app, and a copy that claims to be version 9.9.9.
ditto "$APP" "$T/inst/ViolinMIDI.app"
ditto "$APP" "$T/new/ViolinMIDI.app"
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString 9.9.9" "$T/new/ViolinMIDI.app/Contents/Info.plist"
codesign --force --deep --sign - "$T/new/ViolinMIDI.app"
ditto -c -k --keepParent "$T/new/ViolinMIDI.app" "$T/new.zip"

BIN="$T/inst/ViolinMIDI.app/Contents/MacOS/ViolinMIDI"
BASE=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$T/inst/ViolinMIDI.app/Contents/Info.plist")
echo "installed version: $BASE"

# Starts the installed app and prints which version came up.
came_up() {
    local out="$T/$1.txt"
    rm -f "$out"
    "$BIN" --write-version-to "$out" || true
    for _ in $(seq 1 150); do
        [ -s "$out" ] && break
        sleep 0.2
    done
    [ -s "$out" ] || { echo "FAILED: no version reported in ($1)" >&2; exit 1; }
    sleep 1 # let that process end before the next start
    head -n 1 "$out"
}

expect() {
    if [ "$2" = "$3" ]; then
        echo "ok: $1 -> $2"
    else
        echo "FAILED: $1: got '$2', expected '$3'"
        exit 1
    fi
}

expect "nothing selected" "$(came_up a)" "$BASE"

"$BIN" --install-zip "$T/new.zip" --select
test -x "$VERSIONS/9.9.9/ViolinMIDI.app/Contents/MacOS/ViolinMIDI"
codesign --verify --deep --strict "$VERSIONS/9.9.9/ViolinMIDI.app"

expect "update selected" "$(came_up b)" "9.9.9"
grep -q "/Versions/9.9.9/ViolinMIDI.app" "$T/b.txt" || { echo "FAILED: the update did not run from the versions folder"; cat "$T/b.txt"; exit 1; }
expect "next start stays on the update" "$(came_up c)" "9.9.9"

"$BIN" --select-version none
expect "back to the installed version" "$(came_up d)" "$BASE"

"$BIN" --select-version 9.9.9
expect "forward again" "$(came_up e)" "9.9.9"

# A version that cannot start must not lock the user out.
: > "$VERSIONS/9.9.9/ViolinMIDI.app/Contents/MacOS/ViolinMIDI"
"$BIN" --write-version-to "$T/f.txt" || true # hands over to the broken copy; nothing comes up
sleep 25                                     # longer than the app waits for a start to report back
if [ -s "$T/f.txt" ]; then
    echo "FAILED: the broken version reported in"
    exit 1
fi
expect "fallback after a failed start" "$(came_up g)" "$BASE"
expect "fallback is kept" "$(came_up h)" "$BASE"

echo "UPDATE TEST OK"
