#!/usr/bin/env bash

# Shared AppDir deployment rules for the Linux packaging scripts. Source this
# file from scripts that have already defined ROOT_DIR.

# BASS libraries are resolved via applicationDirPath(), so they must be beside
# the executable. usr/lib keeps them visible to the dynamic linker as well.
deploy_bass_libraries() {
  local build_dir="$1"
  local appdir="$2"
  local lib

  for lib in "$build_dir"/bin/libbass*.so*; do
    [[ -e "$lib" ]] || continue
    cp -L "$lib" "$appdir/usr/bin/" 2>/dev/null || true
    cp -L "$lib" "$appdir/usr/lib/" 2>/dev/null || true
  done
}
