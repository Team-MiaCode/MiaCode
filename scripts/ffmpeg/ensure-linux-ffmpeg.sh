#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FFMPEG_DIR="${MIACODE_LINUX_FFMPEG_DIR:-$ROOT_DIR/third_party/ffmpeg/linux}"
FFMPEG_PATH="$FFMPEG_DIR/ffmpeg"
CACHE_DIR="${MIACODE_LINUX_FFMPEG_CACHE_DIR:-$ROOT_DIR/build/ffmpeg}"

readonly FFMPEG_ASSET_PATTERN='^ffmpeg-n7\.1\..*-linux64-gpl-7\.1\.tar\.xz$'
readonly EXPECTED_VERSION_PATTERN='^ffmpeg version n?7\.1\.5([.-]|$)'

require_command() {
  local command_name="$1"
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Required command not found: $command_name" >&2
    exit 1
  fi
}

require_output_token() {
  local output="$1"
  local token="$2"
  local category="$3"
  if ! grep -Eq "(^|[[:space:]])${token}([[:space:]]|$)" <<<"$output"; then
    echo "FFmpeg is missing required ${category}: ${token}" >&2
    return 1
  fi
}

validate_binary() {
  local binary_path="$1"
  if [[ ! -f "$binary_path" || ! -s "$binary_path" || ! -x "$binary_path" ]]; then
    return 1
  fi

  local file_description
  file_description="$(file -L "$binary_path")"
  if ! grep -q "ELF 64-bit" <<<"$file_description" || ! grep -q "x86-64" <<<"$file_description"; then
    echo "FFmpeg is not a Linux x86-64 ELF binary: $file_description" >&2
    return 1
  fi
  local runtime_dependencies
  runtime_dependencies="$(ldd "$binary_path" 2>&1)"
  if grep -q "not found" <<<"$runtime_dependencies"; then
    echo "FFmpeg has unresolved runtime dependencies:" >&2
    grep "not found" <<<"$runtime_dependencies" >&2
    return 1
  fi
  if grep -Eq 'lib(avcodec|avdevice|avfilter|avformat|avutil|postproc|swresample|swscale)\.so' \
    <<<"$runtime_dependencies"; then
    echo "FFmpeg unexpectedly depends on system FFmpeg shared libraries:" >&2
    echo "$runtime_dependencies" >&2
    return 1
  fi

  local version_line
  version_line="$("$binary_path" -hide_banner -version 2>/dev/null | head -n 1)"
  if ! grep -Eq "$EXPECTED_VERSION_PATTERN" <<<"$version_line"; then
    echo "Unexpected FFmpeg version: ${version_line:-<empty>}" >&2
    return 1
  fi

  local encoders filters protocols formats
  encoders="$("$binary_path" -hide_banner -encoders 2>/dev/null)"
  filters="$("$binary_path" -hide_banner -filters 2>/dev/null)"
  protocols="$("$binary_path" -hide_banner -protocols 2>/dev/null)"
  formats="$("$binary_path" -hide_banner -formats 2>/dev/null)"

  require_output_token "$encoders" "libx264" "encoder"
  require_output_token "$encoders" "aac" "encoder"
  for filter_name in \
    scale crop pad fps format overlay fade color setpts tpad alphamerge \
    atrim asetpts aresample aformat concat amix; do
    require_output_token "$filters" "$filter_name" "filter"
  done
  for protocol_name in file pipe; do
    require_output_token "$protocols" "$protocol_name" "protocol"
  done
  require_output_token "$formats" "mp4" "format"
  require_output_token "$formats" "rawvideo" "format"

  echo "Validated Linux FFmpeg: $binary_path"
  echo "  $version_line"
}

# BtbN retires individual autobuild tags, so a hard-coded release URL rots.
# Query the GitHub API for the newest autobuild that ships an n7.1 linux64-gpl
# archive and download that instead. The asset is accepted based purely on the
# runtime checks in validate_binary, so no archive hash is pinned.
discover_ffmpeg_asset() {
  local api="https://api.github.com/repos/BtbN/FFmpeg-Builds/releases"
  local json_file
  json_file="$(mktemp)"
  if ! curl --fail --silent --show-error --location --http1.1 --retry 5 --retry-all-errors \
    --retry-delay 2 -H "Accept: application/vnd.github+json" "$api?per_page=20" -o "$json_file"; then
    rm -f "$json_file"
    return 1
  fi
  MIACODE_FFMPEG_ASSET_PATTERN="$FFMPEG_ASSET_PATTERN" python3 -c '
import json, os, re, sys

pattern = re.compile(os.environ["MIACODE_FFMPEG_ASSET_PATTERN"])
with open(sys.argv[1], encoding="utf-8") as stream:
    releases = json.load(stream)
for release in releases:
    if not release.get("tag_name", "").startswith("autobuild-"):
        continue
    for asset in release.get("assets", []):
        name = asset.get("name", "")
        if pattern.match(name):
            print(name)
            print(asset["browser_download_url"])
            sys.exit(0)
sys.exit(1)
' "$json_file"
  local status=$?
  rm -f "$json_file"
  return $status
}

require_command curl
require_command file
require_command ldd
require_command tar

if validate_binary "$FFMPEG_PATH"; then
  exit 0
fi

mkdir -p "$CACHE_DIR"

mapfile -t discovered < <(discover_ffmpeg_asset)
if [[ "${#discovered[@]}" -lt 2 ]]; then
  echo "No Linux FFmpeg archive matching $FFMPEG_ASSET_PATTERN found in BtbN/FFmpeg-Builds" >&2
  exit 1
fi
FFMPEG_ARCHIVE_NAME="${discovered[0]}"
FFMPEG_URL="${discovered[1]}"
ARCHIVE_PATH="$CACHE_DIR/$FFMPEG_ARCHIVE_NAME"
EXTRACT_DIR="$CACHE_DIR/ffmpeg-extract"

if [[ ! -s "$ARCHIVE_PATH" ]]; then
  download_path="$ARCHIVE_PATH.part"
  echo "Downloading Linux FFmpeg from $FFMPEG_URL"
  rm -f "$download_path"
  curl \
    --fail \
    --location \
    --http1.1 \
    --retry 5 \
    --retry-all-errors \
    --retry-delay 2 \
    --output "$download_path" \
    "$FFMPEG_URL"
  mv -f "$download_path" "$ARCHIVE_PATH"
fi

rm -rf "$EXTRACT_DIR"
mkdir -p "$EXTRACT_DIR"
tar -xJf "$ARCHIVE_PATH" -C "$EXTRACT_DIR"

mapfile -t downloaded_paths < <(find "$EXTRACT_DIR" -type f -name ffmpeg -print)
if [[ "${#downloaded_paths[@]}" -ne 1 ]]; then
  echo "FFmpeg binary was not found in $ARCHIVE_PATH" >&2
  exit 1
fi
downloaded_path="${downloaded_paths[0]}"

mkdir -p "$FFMPEG_DIR"
install -m 755 "$downloaded_path" "$FFMPEG_PATH"
validate_binary "$FFMPEG_PATH"

echo "Prepared Linux FFmpeg at $FFMPEG_PATH"
echo "Download cache retained at $CACHE_DIR"
