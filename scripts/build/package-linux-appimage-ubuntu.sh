#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-linux-ubuntu}"
QT_ROOT="${QT_ROOT:-}"
MIACODE_BUILD_DEV_TOOLS="${MIACODE_BUILD_DEV_TOOLS:-OFF}"
RELEASE_CHANNEL="${MIACODE_RELEASE_CHANNEL:-nightly}"
PACKAGE_CHANNEL="${MIACODE_PACKAGE_CHANNEL-$RELEASE_CHANNEL}"
if [[ "$PACKAGE_CHANNEL" == "release" ]]; then
  PACKAGE_CHANNEL=""
fi
PACKAGE_JOBS="${MIACODE_PACKAGE_JOBS:-$(nproc 2>/dev/null || echo 4)}"

if [[ -z "$QT_ROOT" && -n "${QT_ROOT_DIR:-}" ]]; then
  QT_ROOT="$QT_ROOT_DIR"
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export DEBIAN_FRONTEND=noninteractive

install_sys_deps() {
  if command -v apt-get >/dev/null 2>&1; then
    # GitHub runners and non-root shells need sudo for apt operations.
    local sudo=""
    if [[ "$(id -u)" -ne 0 ]] && command -v sudo >/dev/null 2>&1; then
      sudo="sudo"
    fi
    $sudo apt-get update -y
    $sudo apt-get install -y \
      build-essential \
      cmake \
      ninja-build \
      pkg-config \
      git \
      git-lfs \
      curl \
      wget \
      patchelf \
      file \
      imagemagick \
      python3 \
      python3-venv \
      libgl1-mesa-dev \
      libegl1-mesa-dev \
      libxkbcommon-dev \
      libxkbcommon-x11-0 \
      libxkbcommon-x11-dev \
      libxcb-cursor0 \
      libxcb-cursor-dev \
      libxcb-xkb-dev \
      libxcb-xinerama0 \
      libxcb-randr0 \
      libxcb-keysyms1 \
      libxcb-image0 \
      libxcb-shm0 \
      libxcb-icccm4 \
      libxcb-sync1 \
      libxcb-xfixes0 \
      libxcb-shape0 \
      libxcb-render-util0 \
      libxcb-util1 \
      libxcb1-dev \
      libpulse-dev \
      libasound2-dev \
      libva-dev \
      libva-drm2 \
      libdrm-dev \
      libavcodec-dev \
      libavformat-dev \
      libavutil-dev \
      libswscale-dev \
      libswresample-dev \
      libavfilter-dev \
      libfuse2 || true \
      fuse || true \
      xz-utils \
      zlib1g-dev
  else
    echo "apt-get not found, skipping system deps" >&2
  fi
}

ensure_aqt() {
  local venv="$HOME/.local/aqt-venv"
  if [[ ! -x "$venv/bin/aqt" ]]; then
    mkdir -p "$HOME/.local"
    python3 -m venv "$venv"
    "$venv/bin/pip" install -q "aqtinstall==3.3.*" "py7zr==1.0.*"
  fi
  echo "$venv/bin/aqt"
}

export_qt_env() {
  # linuxdeploy-plugin-qt locates Qt through qmake, and the build tools
  # (qmake, moc, rcc, uic) must resolve to this Qt, not a system Qt5.
  local qt_prefix="$1"
  export QT_ROOT_DIR="$qt_prefix"
  export QT_ROOT="$qt_prefix"
  export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-$qt_prefix}"
  if [[ -x "$qt_prefix/bin/qmake6" ]]; then
    export QMAKE="$qt_prefix/bin/qmake6"
  elif [[ -x "$qt_prefix/bin/qmake" ]]; then
    export QMAKE="$qt_prefix/bin/qmake"
  fi
  export PATH="$qt_prefix/bin:$PATH"
}

ensure_qt() {
  if [[ -n "$QT_ROOT" && -d "$QT_ROOT" && (-x "$QT_ROOT/bin/qmake6" || -x "$QT_ROOT/bin/qmake") ]]; then
    echo "Using existing Qt at $QT_ROOT"
    export_qt_env "$QT_ROOT"
    return
  fi
  local aqt_bin
  aqt_bin="$(ensure_aqt)"
  QT_ROOT="$ROOT_DIR/.qt/6.11.1/gcc_64"
  if [[ -x "$QT_ROOT/bin/qmake6" || -x "$QT_ROOT/bin/qmake" ]]; then
    echo "Qt already present at $QT_ROOT"
  else
    "$aqt_bin" install-qt linux desktop 6.11.1 linux_gcc_64 --outputdir "$ROOT_DIR/.qt" --modules qtmultimedia qtshadertools
  fi
  export_qt_env "$QT_ROOT"
}

ensure_ffmpeg() {
  local ffmpeg_dir="$ROOT_DIR/third_party/ffmpeg/linux"
  mkdir -p "$ffmpeg_dir"
  if [[ -x "$ffmpeg_dir/ffmpeg" && -s "$ffmpeg_dir/ffmpeg" ]]; then
    echo "FFmpeg exists at $ffmpeg_dir/ffmpeg"
    return
  fi
  if bash "$ROOT_DIR/scripts/ffmpeg/ensure-linux-ffmpeg.sh" 2>/dev/null; then
    return
  fi
  echo "ensure-linux-ffmpeg.sh failed, falling back to system ffmpeg"
  if command -v ffmpeg >/dev/null 2>&1; then
    cp "$(command -v ffmpeg)" "$ffmpeg_dir/ffmpeg"
    chmod +x "$ffmpeg_dir/ffmpeg"
  else
    echo "No ffmpeg available" >&2
    exit 1
  fi
}

ensure_linuxdeploy() {
  mkdir -p "$HOME/.local/bin"
  if [[ -x "$HOME/.local/bin/linuxdeploy" ]]; then
    chmod +x "$HOME/.local/bin/linuxdeploy"
    echo "$HOME/.local/bin/linuxdeploy"
    return
  fi
  curl -L --retry 3 -o "$HOME/.local/bin/linuxdeploy" "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage"
  chmod +x "$HOME/.local/bin/linuxdeploy"
  echo "$HOME/.local/bin/linuxdeploy"
}

ensure_linuxdeploy_plugin_qt() {
  mkdir -p "$HOME/.local/bin"
  if [[ -x "$HOME/.local/bin/linuxdeploy-plugin-qt" ]]; then
    chmod +x "$HOME/.local/bin/linuxdeploy-plugin-qt"
    echo "$HOME/.local/bin/linuxdeploy-plugin-qt"
    return
  fi
  curl -L --retry 3 -o "$HOME/.local/bin/linuxdeploy-plugin-qt" "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage"
  chmod +x "$HOME/.local/bin/linuxdeploy-plugin-qt"
  echo "$HOME/.local/bin/linuxdeploy-plugin-qt"
}

ensure_appimagetool() {
  local tool="$HOME/.local/bin/appimagetool"
  mkdir -p "$HOME/.local/bin"
  # The old AppImageKit "13" asset was removed upstream and now returns a plain
  # "Not Found" body that fails to execute; the maintained repo hosts the binary.
  if [[ -x "$tool" ]] && head -c 4 "$tool" 2>/dev/null | grep -q ELF; then
    chmod +x "$tool"
    echo "$tool"
    return
  fi
  rm -f "$tool"
  curl -L --retry 3 -o "$tool" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
  if ! head -c 4 "$tool" 2>/dev/null | grep -q ELF; then
    echo "Failed to download a valid appimagetool from $tool" >&2
    return 1
  fi
  chmod +x "$tool"
  echo "$tool"
}

export PATH="$HOME/.local/bin:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1

# linuxdeploy-plugin-qt only bundles QML modules when it can run qmlimportscanner
# from QT_INSTALL_BINS, which fails on distributions that ship the scanner
# elsewhere. Deploy the modules deterministically instead of trusting the plugin:
# scan the QML sources for imports and copy every resolved module out of
# QT_INSTALL_QML into the AppDir.
deploy_qml_modules() {
  local appdir="$1"
  local qml_root="$2"
  local sources="$3"

  local scanner=""
  # aqt Qt ships qmlimportscanner under libexec/ rather than bin/, so probe both
  # (plus QT_INSTALL_LIBEXECS) before giving up on explicit QML deployment.
  local scanner_dirs=()
  if [[ -n "${QT_ROOT:-}" ]]; then
    scanner_dirs+=("$QT_ROOT/bin" "$QT_ROOT/libexec")
  fi
  if [[ -n "${QMAKE:-}" ]]; then
    local libexecs
    libexecs="$("$QMAKE" -query QT_INSTALL_LIBEXECS 2>/dev/null || true)"
    [[ -n "$libexecs" ]] && scanner_dirs+=("$libexecs")
  fi
  local dir
  for dir in "${scanner_dirs[@]}"; do
    if [[ -x "$dir/qmlimportscanner" ]]; then
      scanner="$dir/qmlimportscanner"
      break
    fi
  done
  if [[ -z "$scanner" ]] && command -v qmlimportscanner >/dev/null 2>&1; then
    scanner="$(command -v qmlimportscanner)"
  fi
  if [[ -z "$scanner" ]]; then
    echo "qmlimportscanner not found; skipping explicit QML module deployment" >&2
    return 0
  fi
  if [[ ! -d "$qml_root" ]]; then
    echo "Qt QML import path not found at $qml_root; skipping QML deployment" >&2
    return 0
  fi

  local relative
  local source_dir
  while IFS= read -r source_dir; do
    [[ -d "$source_dir" ]] || continue
    local scanned
    scanned="$("$scanner" -rootPath "$source_dir" -importPath "$qml_root" 2>/dev/null || true)"
    [[ -n "$scanned" ]] || continue
    while IFS= read -r relative; do
      [[ -n "$relative" ]] || continue
      local module_src="$qml_root/$relative"
      local module_dst="$appdir/usr/qml/$relative"
      [[ -d "$module_src" ]] || continue
      [[ -e "$module_dst" ]] && continue
      mkdir -p "$(dirname "$module_dst")"
      cp -R "$module_src" "$module_dst"
    done < <(printf '%s' "$scanned" | python3 -c '
import json, sys

try:
    entries = json.load(sys.stdin)
except json.JSONDecodeError:
    sys.exit(0)
seen = set()
for entry in entries:
    relative = entry.get("relativePath")
    if relative and relative not in seen:
        seen.add(relative)
        print(relative)
')
  done < <(printf '%s' "$sources" | tr ':' '\n')
}

install_sys_deps
ensure_qt
ensure_ffmpeg
ensure_linuxdeploy
ensure_linuxdeploy_plugin_qt
ensure_appimagetool

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

if [[ -d "$BUILD_DIR/bin/assets" ]]; then
  cp -R "$BUILD_DIR/bin/assets" "$APPDIR/usr/bin/"
elif [[ -d "$BUILD_DIR/assets" ]]; then
  cp -R "$BUILD_DIR/assets" "$APPDIR/usr/bin/"
elif [[ -d "$ROOT_DIR/assets" ]]; then
  mkdir -p "$APPDIR/usr/share"
  cp -R "$ROOT_DIR/assets" "$APPDIR/usr/share/"
fi

# Copy BASS shared libraries next to the binary (the app checks runtimeFilePath
# relative to applicationDirPath()) and into the library dir for the dynamic linker.
for lib in "$BUILD_DIR/bin"/libbass*.so*; do
  [[ -e "$lib" ]] || continue
  cp -L "$lib" "$APPDIR/usr/bin/" 2>/dev/null || true
  cp -L "$lib" "$APPDIR/usr/lib/" 2>/dev/null || true
done

for lib in "$BUILD_DIR/bin"/*.so*; do
  [[ -e "$lib" ]] || continue
  cp -L "$lib" "$APPDIR/usr/lib/" 2>/dev/null || true
done

for f in LICENSE THIRD_PARTY_NOTICES.md; do
  [[ -f "$ROOT_DIR/$f" ]] && cp "$ROOT_DIR/$f" "$DIST_DIR/"
done
[[ -d "$ROOT_DIR/licenses" ]] && cp -R "$ROOT_DIR/licenses" "$DIST_DIR/licenses"

ICON_SOURCE="$ROOT_DIR/resources/icons/app.png"
ICON_TARGET="$APPDIR/usr/share/icons/hicolor/512x512/apps/miacode.png"
if [[ -f "$ICON_SOURCE" ]]; then
  # linuxdeploy validates that a "512x512" icon really is 512 pixels wide, and
  # the source asset is 2048x2048, so scaling is mandatory rather than cosmetic.
  if command -v magick >/dev/null 2>&1; then
    magick "$ICON_SOURCE" -resize 512x512 "$ICON_TARGET"
  elif command -v convert >/dev/null 2>&1; then
    convert "$ICON_SOURCE" -resize 512x512 "$ICON_TARGET"
  else
    echo "ImageMagick not found; using icon source without scaling" >&2
    cp "$ICON_SOURCE" "$ICON_TARGET"
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

FFMPEG_SRC="$ROOT_DIR/third_party/ffmpeg/linux/ffmpeg"
if [[ -f "$FFMPEG_SRC" && -x "$FFMPEG_SRC" ]]; then
  mkdir -p "$APPDIR/usr/bin/ffmpeg"
  cp "$FFMPEG_SRC" "$APPDIR/usr/bin/ffmpeg/ffmpeg"
  chmod +x "$APPDIR/usr/bin/ffmpeg/ffmpeg"
fi

package_step "Deploying with linuxdeploy"
LINUXDEPLOY_BIN="$(command -v linuxdeploy || echo "$HOME/.local/bin/linuxdeploy")"
APPIMAGETOOL_BIN="$(command -v appimagetool || echo "$HOME/.local/bin/appimagetool")"

export APPIMAGE_EXTRACT_AND_RUN=1
if [[ -n "${QMAKE:-}" ]]; then
  export QMAKE
fi

# linuxdeploy's bundled strip predates RELR sections and aborts on libraries
# from a recent toolchain; rpath rewriting with the bundled patchelf corrupts
# some modern libraries. Use the system patchelf and skip stripping.
export NO_STRIP="${NO_STRIP:-1}"
if [[ -z "${PATCHELF:-}" ]] && command -v patchelf >/dev/null 2>&1; then
  export PATCHELF="$(command -v patchelf)"
fi

# The application embeds its own QML in Qt resources, so the plugin cannot find
# imports by scanning the AppDir; point it at the source trees instead.
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS:-$ROOT_DIR/src/app/ui:$ROOT_DIR/src/intro/qml:$ROOT_DIR/src/preview/stage_media/qml}"

# linuxdeploy's AppRun execs AppRun.wrapped and lets Qt resolve its prefix to
# the AppDir root, which loses the plugin and QML import paths. Install a custom
# AppRun that launches the binary with those paths set explicitly.
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

"$LINUXDEPLOY_BIN" \
  --appdir="$APPDIR" \
  --executable="$APPDIR/usr/bin/MiaCode" \
  --desktop-file="$APPDIR/usr/share/applications/miacode.desktop" \
  --icon-file="$APPDIR/usr/share/icons/hicolor/512x512/apps/miacode.png" \
  --custom-apprun="$DIST_DIR/AppRun" \
  --plugin qt

# The Qt plugin's QML deployment is best-effort; make sure the modules the UI
# imports actually made it in, regardless of what the plugin decided to do.
QML_IMPORT_ROOT=""
if [[ -n "${QT_ROOT:-}" && -d "$QT_ROOT/qml" ]]; then
  QML_IMPORT_ROOT="$QT_ROOT/qml"
elif [[ -n "${QMAKE:-}" ]]; then
  QML_IMPORT_ROOT="$("$QMAKE" -query QT_INSTALL_QML 2>/dev/null || true)"
fi
package_step "Deploying QML modules from ${QML_IMPORT_ROOT:-<unknown>}"
deploy_qml_modules "$APPDIR" "$QML_IMPORT_ROOT" \
  "$ROOT_DIR/src/app/ui:$ROOT_DIR/src/intro/qml:$ROOT_DIR/src/preview/stage_media/qml"

# Fail loudly here rather than shipping an AppImage that cannot load its UI.
if [[ ! -d "$APPDIR/usr/qml/QtQuick/Controls" ]]; then
  echo "QML modules were not deployed into the AppDir; the AppImage would fail to start" >&2
  exit 1
fi

package_step "Building AppImage"
TARGET_APPIMAGE="${DIST_DIR}/${PACKAGE_NAME}.AppImage"
mkdir -p "$(dirname "$TARGET_APPIMAGE")"
"$APPIMAGETOOL_BIN" "$APPDIR" "$TARGET_APPIMAGE"
chmod +x "$TARGET_APPIMAGE"

package_step "Packaging complete"
echo "AppImage created: $TARGET_APPIMAGE"
echo "Package dir: $DIST_DIR"
