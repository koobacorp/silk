#!/bin/bash
# SILK installer: copies SILK.vst3 and SILK.component into the system plug-in folders.
# Asks before replacing an existing copy and keeps a dated backup. Needs your Mac password (sudo).
set -euo pipefail
cd "$(dirname "$0")"
stamp="$(date +%Y%m%d-%H%M%S)"
install_one() {
  local src="$1" dest_dir="$2" name; name="$(basename "$src")"
  [[ -d "$src" ]] || { echo "Keep this script next to $name."; exit 1; }
  local dest="$dest_dir/$name"
  sudo mkdir -p "$dest_dir"
  if [[ -e "$dest" ]]; then
    echo "$name is already installed in $dest_dir."
    read -r -p "Replace it (a backup is kept)? Type yes: " answer
    if [[ "$answer" != yes ]]; then echo "Skipped $name."; return; fi
    sudo mv "$dest" "$HOME/Desktop/$name.backup-$stamp"
    echo "Old copy moved to ~/Desktop/$name.backup-$stamp"
  fi
  sudo cp -R "$src" "$dest"
  sudo xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
  codesign --verify --deep --strict "$dest"
  echo "Installed $dest"
}
echo "Close FL Studio before installing."
install_one "$PWD/SILK.vst3" "/Library/Audio/Plug-Ins/VST3"
if [[ -d "$PWD/SILK.component" ]]; then
  install_one "$PWD/SILK.component" "/Library/Audio/Plug-Ins/Components"
  killall -9 AudioComponentRegistrar 2>/dev/null || true
fi
echo
echo "Done. In FL Studio: Options > Manage plugins > Find plugins,"
echo "then add SILK on a mixer track (it is an effect, not a generator)."
