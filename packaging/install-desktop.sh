#!/usr/bin/env bash
#
# Install SoundPlayer2 into the user's desktop environment:
#   - symlinks ~/.local/bin/soundplayer2 to the newest local build artifact
#   - installs the app icon, .desktop launcher and module MIME types
#   - registers SoundPlayer2 as the default handler for its audio types
#
# No sudo required. Re-run after switching build directories or moving the repo.

set -euo pipefail

# Resolve paths relative to this script so it works from any CWD.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

BINARY=""
CANDIDATES=(
    "$PROJECT_DIR/build/SoundPlayer2_artefacts/SoundPlayer2"
    "$PROJECT_DIR/build/SoundPlayer2_artefacts/Release/SoundPlayer2"
    "$PROJECT_DIR/build/SoundPlayer2_artefacts/Debug/SoundPlayer2"
    "$PROJECT_DIR/cmake-build-debug/SoundPlayer2_artefacts/Debug/SoundPlayer2"
    "$PROJECT_DIR/cmake-build-release/SoundPlayer2_artefacts/Release/SoundPlayer2"
)

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
BIN_DIR="$HOME/.local/bin"
APP_DIR="$DATA_HOME/applications"
ICON_DIR="$DATA_HOME/icons/hicolor/scalable/apps"
MIME_DIR="$DATA_HOME/mime/packages"

DESKTOP_FILE="soundplayer2.desktop"
MIME_TYPES=(
    audio/mpeg audio/x-wav audio/wav audio/flac
    audio/ogg audio/x-vorbis+ogg
    audio/x-soundplayer2-module
    audio/x-mod audio/x-xm audio/x-it audio/x-s3m
    audio/x-extracker-xtp
)

latest_mtime=0
for candidate in "${CANDIDATES[@]}"; do
    if [[ -x "$candidate" ]]; then
        candidate_mtime=$(stat -c '%Y' "$candidate")
        if (( candidate_mtime > latest_mtime )); then
            latest_mtime=$candidate_mtime
            BINARY="$candidate"
        fi
    fi
done

if [[ -z "$BINARY" ]]; then
    echo "error: built binary not found at:" >&2
    for candidate in "${CANDIDATES[@]}"; do
        echo "  $candidate" >&2
    done
    echo "Build it first, e.g.:" >&2
    echo "  (cd \"$PROJECT_DIR\" && cmake -S . -B build && cmake --build build --config Release)" >&2
    exit 1
fi

mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR" "$MIME_DIR"

echo "Linking launcher -> $BIN_DIR/soundplayer2"
rm -f "$BIN_DIR/soundplayer2"
ln -s "$BINARY" "$BIN_DIR/soundplayer2"

echo "Installing icon -> $ICON_DIR/soundplayer2.svg"
install -m 0644 "$SCRIPT_DIR/soundplayer2.svg" "$ICON_DIR/soundplayer2.svg"

echo "Installing MIME types -> $MIME_DIR/soundplayer2.xml"
install -m 0644 "$SCRIPT_DIR/soundplayer2-mime.xml" "$MIME_DIR/soundplayer2.xml"
if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database "$DATA_HOME/mime" >/dev/null 2>&1 || true
fi

echo "Installing launcher -> $APP_DIR/$DESKTOP_FILE"
install -m 0644 "$SCRIPT_DIR/$DESKTOP_FILE" "$APP_DIR/$DESKTOP_FILE"
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi

if command -v xdg-mime >/dev/null 2>&1; then
    echo "Registering default file associations"
    for mt in "${MIME_TYPES[@]}"; do
        xdg-mime default "$DESKTOP_FILE" "$mt" 2>/dev/null || true
    done
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$DATA_HOME/icons/hicolor" >/dev/null 2>&1 || true
fi

echo
echo "Done. SoundPlayer2 should now appear in your application menu."
echo "Desktop launcher target: $BINARY"

case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *)
        echo
        echo "note: $BIN_DIR is not on your PATH."
        echo "      The menu entry still works; to run 'soundplayer2' from a shell, add:"
        echo "        export PATH=\"\$HOME/.local/bin:\$PATH\""
        ;;
esac
