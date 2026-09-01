#!/usr/bin/env bash
#
# Remove the desktop integration installed by install-desktop.sh.

set -euo pipefail

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
BIN_DIR="$HOME/.local/bin"
APP_DIR="$DATA_HOME/applications"
ICON_DIR="$DATA_HOME/icons/hicolor/scalable/apps"
MIME_DIR="$DATA_HOME/mime/packages"

rm -f "$BIN_DIR/soundplayer2"
rm -f "$ICON_DIR/soundplayer2.svg"
rm -f "$MIME_DIR/soundplayer2.xml"
rm -f "$APP_DIR/soundplayer2.desktop"

if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database "$DATA_HOME/mime" >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$DATA_HOME/icons/hicolor" >/dev/null 2>&1 || true
fi

echo "Removed SoundPlayer2 desktop integration."
