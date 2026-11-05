#!/usr/bin/env bash
# Linux local build entry for AppImage packaging
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
QT_VERSION="${QT_VERSION:-6.11.1}"
QT_ROOT="${QT_ROOT:-${QT_ROOT_DIR:-}}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-linux}"
DEPLOYMENT_TARGET=""
MIACODE_BUILD_DEV_TOOLS="${MIACODE_BUILD_DEV_TOOLS:-OFF}"
FFMPEG_EXPORT_BINARY="$ROOT_DIR/third_party/ffmpeg/linux/ffmpeg"
RUNNER_ARCH="$(uname -m)"

# Ensure export ffmpeg
if [[ ! -s "$FFMPEG_EXPORT_BINARY" ]]; then
  echo "Missing Linux export ffmpeg binary: $FFMPEG_EXPORT_BINARY" >&2
  echo "Run: bash scripts/ffmpeg/ensure-linux-ffmpeg.sh" >&2
  exit 1
fi
chmod +x "$FFMPEG_EXPORT_BINARY"

# Qt detection
if [[ -z "$QT_ROOT" ]]; then
  for candidate in "$ROOT_DIR/.qt/$QT_VERSION/gcc_64" "$HOME/Qt/$QT_VERSION/gcc_64"; do
    if [[ -x "$candidate/bin/qmake" || -x "$candidate/bin/qmake6" ]]; then
      QT_ROOT="$candidate"
      break
    fi
  done
fi
# Fall back to the distro Qt when no pinned toolchain is installed. Only Qt
# versions accepted by CMakeLists.txt will configure, and anything below it
# fails there with a clear message.
if [[ -z "$QT_ROOT" ]] && command -v qmake6 >/dev/null 2>&1; then
  QT_ROOT="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
  if [[ -n "$QT_ROOT" ]]; then
    echo "Using system Qt at $QT_ROOT (qmake6 -query QT_INSTALL_PREFIX)"
  fi
fi
if [[ -z "$QT_ROOT" ]]; then
  echo "No Qt $QT_VERSION found for Linux build." >&2
  echo "Set QT_ROOT to Qt $QT_VERSION gcc_64 directory, or install via aqt." >&2
  exit 1
fi

export QT_ROOT_DIR="$QT_ROOT"
export BUILD_DIR
export MIACODE_BUILD_DEV_TOOLS
export CMAKE_BUILD_TYPE="Release"
export CMAKE_PREFIX_PATH="${QT_ROOT}"

chmod +x "$ROOT_DIR/scripts/build/package-linux-appimage.sh"
bash "$ROOT_DIR/scripts/build/package-linux-appimage.sh"
