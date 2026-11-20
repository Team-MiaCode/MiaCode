#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-linux}"
QT_ROOT="${QT_ROOT:-}"
MIACODE_BUILD_DEV_TOOLS="${MIACODE_BUILD_DEV_TOOLS:-OFF}"
RELEASE_CHANNEL="${MIACODE_RELEASE_CHANNEL:-nightly}"
PACKAGE_CHANNEL="${MIACODE_PACKAGE_CHANNEL-$RELEASE_CHANNEL}"
if [[ "$PACKAGE_CHANNEL" == "release" ]]; then
  PACKAGE_CHANNEL=""
fi
PACKAGE_JOBS="${MIACODE_PACKAGE_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

if [[ -z "$QT_ROOT" && -n "${QT_ROOT_DIR:-}" ]]; then
  QT_ROOT="$QT_ROOT_DIR"
fi
if [[ -n "$QT_ROOT" ]]; then
  export PATH="$QT_ROOT/bin:$PATH"
  export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-$QT_ROOT}"
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found in PATH" >&2
  exit 1
fi
if ! command -v linuxdeploy >/dev/null 2>&1 && ! command -v linuxdeploy-x86_64 >/dev/null 2>&1; then
  echo "linuxdeploy not found. Please install linuxdeploy and plugins, or set PATH." >&2
  exit 1
fi
if ! command -v appimagetool >/dev/null 2>&1 && ! command -v appimagetool-x86_64 >/dev/null 2>/dev/null; then
  echo "appimagetool not found. Please install appimagetool." >&2
  exit 1
fi

parse_version() {
  local cmake_file="$1"
  major="$(grep -Eo 'set\(MIACODE_VERSION_MAJOR\s+"[^"]+"' "$cmake_file" | sed -E 's/.*"([^"]+)"/\1/')"
  minor="$(grep -Eo 'set\(MIACODE_VERSION_MINOR\s+"[^"]+"' "$cmake_file" | sed -E 's/.*"([^"]+)"/\1/')"
  patch="$(grep -Eo 'set\(MIACODE_VERSION_PATCH\s+"[^"]+"' "$cmake_file" | sed -E 's/.*"([^"]+)"/\1/')"
  prerelease="$(grep -Eo 'set\(MIACODE_VERSION_PRERELEASE\s+"[^"]*"' "$cmake_file" | sed -E 's/.*"([^"]*)"/\1/' || true)"
  version="${major}.${minor}.${patch}"
  [[ -n "$prerelease" ]] && version="${version}-${prerelease}"
  echo "$version"
}

package_step() {
  printf '\n==> [package] %s\n' "$*"
}

VERSION="$(parse_version "$ROOT_DIR/CMakeLists.txt")"
PACKAGE_NAME="MiaCode_${VERSION}"
if [[ -n "$PACKAGE_CHANNEL" ]]; then
  PACKAGE_NAME+="_${PACKAGE_CHANNEL}"
fi
PACKAGE_NAME+="_linux_x86_64"
DIST_DIR="${DIST_DIR:-$ROOT_DIR/dist/$PACKAGE_NAME}"
APPDIR="${DIST_DIR}/AppDir"

package_step "Configuring Release build in $BUILD_DIR"
cmake_args=(
  -S "$ROOT_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Release
  "-DMIACODE_RELEASE_CHANNEL=$RELEASE_CHANNEL"
  "-DMIACODE_BUILD_DEV_TOOLS=$MIACODE_BUILD_DEV_TOOLS"
)
if [[ -n "$QT_ROOT" ]]; then
  cmake_args+=("-DCMAKE_PREFIX_PATH=$QT_ROOT")
fi
cmake "${cmake_args[@]}"

package_step "Building MiaCode"
build_args=(--build "$BUILD_DIR" --config Release --parallel "$PACKAGE_JOBS" --target MiaCode)
cmake "${build_args[@]}"

APP_BINARY=""
for candidate in "$BUILD_DIR/bin/MiaCode" "$BUILD_DIR/MiaCode"; do
  if [[ -f "$candidate" && -x "$candidate" ]]; then
    APP_BINARY="$candidate"
    break
  fi
done
if [[ -z "$APP_BINARY" ]]; then
  echo "MiaCode binary not found in $BUILD_DIR" >&2
  exit 1
fi

package_step "Preparing AppDir"
rm -rf "$DIST_DIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/icons/hicolor/512x512/apps" "$APPDIR/usr/share/applications"

cp "$APP_BINARY" "$APPDIR/usr/bin/MiaCode"
chmod +x "$APPDIR/usr/bin/MiaCode"

# Copy assets and runtime dependencies (BASS, FFmpeg runtime if bundled next to binary)
if [[ -d "$BUILD_DIR/bin/assets" ]]; then
  cp -R "$BUILD_DIR/bin/assets" "$APPDIR/usr/bin/"
elif [[ -d "$BUILD_DIR/assets" ]]; then
  cp -R "$BUILD_DIR/assets" "$APPDIR/usr/bin/"
elif [[ -d "$ROOT_DIR/assets" ]]; then
  cp -R "$ROOT_DIR/assets" "$APPDIR/usr/share/"
fi

# Copy shared libs placed next to binary by CMake (BASS .so)
for lib in "$BUILD_DIR/bin"/*.so*; do
  [[ -e "$lib" ]] || continue
  cp -L "$lib" "$APPDIR/usr/lib/" 2>/dev/null || true
done

# Copy license files
for f in LICENSE THIRD_PARTY_NOTICES.md; do
  [[ -f "$ROOT_DIR/$f" ]] && cp "$ROOT_DIR/$f" "$DIST_DIR/"
done
[[ -d "$ROOT_DIR/licenses" ]] && cp -R "$ROOT_DIR/licenses" "$DIST_DIR/licenses"

# Icon and desktop file. linuxdeploy only accepts hicolor icons at standard
# resolutions, so scale the 2048px source down to the 512x512 slot.
ICON_SOURCE="$ROOT_DIR/resources/icons/app.png"
ICON_TARGET="$APPDIR/usr/share/icons/hicolor/512x512/apps/miacode.png"
if [[ -f "$ICON_SOURCE" ]]; then
  if command -v magick >/dev/null 2>&1; then
    magick "$ICON_SOURCE" -resize 512x512 "$ICON_TARGET"
  elif command -v convert >/dev/null 2>&1; then
    convert "$ICON_SOURCE" -resize 512x512 "$ICON_TARGET"
  else
    echo "ImageMagick (magick or convert) is required to resize $ICON_SOURCE" >&2
    exit 1
  fi
fi

cat > "$APPDIR/usr/share/applications/miacode.desktop" <<'EOF'
[Desktop Entry]
Name=MiaCode
Exec=MiaCode
Type=Application
Icon=miacode
Categories=Graphics;AudioVideo;
Terminal=false
EOF

# Copy export ffmpeg into AppDir
FFMPEG_SRC="$ROOT_DIR/third_party/ffmpeg/linux/ffmpeg"
if [[ -f "$FFMPEG_SRC" && -x "$FFMPEG_SRC" ]]; then
  mkdir -p "$APPDIR/usr/bin/ffmpeg"
  cp "$FFMPEG_SRC" "$APPDIR/usr/bin/ffmpeg/ffmpeg"
  chmod +x "$APPDIR/usr/bin/ffmpeg/ffmpeg"
fi

package_step "Deploying with linuxdeploy"
LINUXDEPLOY_BIN="$(command -v linuxdeploy-x86_64 || command -v linuxdeploy)"
APPIMAGETOOL_BIN="$(command -v appimagetool-x86_64 || command -v appimagetool)"

export APPIMAGE_EXTRACT_AND_RUN=1

# linuxdeploy-plugin-qt picks the first qmake on PATH, which is a Qt 5 one on
# distros that ship both generations (Arch: /usr/bin/qmake -> Qt 5). Pin it to
# the Qt that is actually being packaged.
if [[ -z "${QMAKE:-}" ]]; then
  if [[ -n "$QT_ROOT" && -x "$QT_ROOT/bin/qmake6" ]]; then
    export QMAKE="$QT_ROOT/bin/qmake6"
  elif command -v qmake6 >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake6)"
  elif [[ -n "$QT_ROOT" && -x "$QT_ROOT/bin/qmake" ]]; then
    export QMAKE="$QT_ROOT/bin/qmake"
  elif command -v qmake >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake)"
  fi
fi

# linuxdeploy's bundled strip is older than RELR sections (.relr.dyn) and
# aborts the whole deployment on libraries built by a recent toolchain.
# Distro Qt libraries are stripped already, so skip that pass.
export NO_STRIP="${NO_STRIP:-1}"

# Same story for rpath rewriting: the bundled patchelf 0.15 corrupts some
# modern libraries (leancrypto 1.9 in particular) while setting $ORIGIN.
if [[ -z "${PATCHELF:-}" ]] && command -v patchelf >/dev/null 2>&1; then
  export PATCHELF="$(command -v patchelf)"
fi

# The application embeds its QML in Qt resources, so the plugin cannot find
# imports by scanning the AppDir; point it at the source trees instead.
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS:-$ROOT_DIR/src/app/ui:$ROOT_DIR/src/intro/qml:$ROOT_DIR/src/preview/stage_media/qml}"

# linuxdeploy's generated AppRun execs AppRun.wrapped, normally a symlink to
# the binary. Qt then resolves its prefix to the AppDir root and starts
# without plugin paths. --custom-apprun installs this script as AppRun.wrapped
# instead, so launch the real binary with explicit plugin/QML paths.
cat > "$DIST_DIR/AppRun" <<'EOF'
#!/usr/bin/env bash
set -e
APPDIR="$(dirname "$(readlink -f "$0")")"
export QT_PLUGIN_PATH="$APPDIR/usr/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QML_IMPORT_PATH="$APPDIR/usr/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
export QML2_IMPORT_PATH="$APPDIR/usr/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
exec "$APPDIR/usr/bin/MiaCode" "$@"
EOF
chmod +x "$DIST_DIR/AppRun"

# Ensure AppRun.wrapped is also fixed (in case linuxdeploy overrides it)
cat > "$DIST_DIR/AppRun.wrapped" <<'EOF'
#!/usr/bin/env bash
set -e
APPDIR="$(dirname "$(readlink -f "$0")")"
export QT_PLUGIN_PATH="$APPDIR/usr/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QML_IMPORT_PATH="$APPDIR/usr/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
export QML2_IMPORT_PATH="$APPDIR/usr/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
exec "$APPDIR/usr/bin/MiaCode" "$@"
EOF
chmod +x "$DIST_DIR/AppRun.wrapped"

"$LINUXDEPLOY_BIN" \
  --appdir="$APPDIR" \
  --executable="$APPDIR/usr/bin/MiaCode" \
  --desktop-file="$APPDIR/usr/share/applications/miacode.desktop" \
  --icon-file="$ICON_TARGET" \
  --custom-apprun="$DIST_DIR/AppRun" \
  --plugin qt \
  --output appimage

# Move generated AppImage to dist
APPIMAGE_OUTPUT="$(find "$ROOT_DIR" -name "MiaCode-*.AppImage" -not -path "*/tmp/*" -not -path "*/build/*" | sort | tail -n 1)"
if [[ -z "$APPIMAGE_OUTPUT" || ! -f "$APPIMAGE_OUTPUT" ]]; then
  # Try finding in current dir
  APPIMAGE_OUTPUT="$(find . -name "MiaCode-*.AppImage" | sort | tail -n 1)"
fi
TARGET_APPIMAGE="${DIST_DIR}/${PACKAGE_NAME}.AppImage"
mkdir -p "$(dirname "$TARGET_APPIMAGE")"
if [[ -n "$APPIMAGE_OUTPUT" && -f "$APPIMAGE_OUTPUT" ]]; then
  mv "$APPIMAGE_OUTPUT" "$TARGET_APPIMAGE"
else
  # Fallback: try appimagetool directly
  "$APPIMAGETOOL_BIN" "$APPDIR" "$TARGET_APPIMAGE"
fi
chmod +x "$TARGET_APPIMAGE"

package_step "Packaging complete"
echo "AppImage created: $TARGET_APPIMAGE"
echo "Package dir: $DIST_DIR"
