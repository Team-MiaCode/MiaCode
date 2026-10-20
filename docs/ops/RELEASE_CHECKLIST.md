# Release Checklist

## Version

- [ ] Update `MIACODE_VERSION_MAJOR`, `MIACODE_VERSION_MINOR`, `MIACODE_VERSION_PATCH`, and `MIACODE_VERSION_PRERELEASE` in [CMakeLists.txt](../../CMakeLists.txt).
- [ ] Confirm the About dialog and package filename use the same version.
- [ ] Add release notes to [CHANGELOG.md](../../CHANGELOG.md).

## Clean Build

- [ ] Start from a clean clone or a clean working tree.
- [ ] Confirm required Qt modules are available: `Core`, `Gui`, `Widgets`, `OpenGL`, `Qml`, `Quick`, `Multimedia`.
- [ ] Confirm FFmpeg runtime/dev SDK provisioning scripts still point at the intended versions.

## Windows Package

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build\build-win.ps1 -Toolchain msvc -BuildDir build-msvc              # x64
powershell -ExecutionPolicy Bypass -File .\scripts\build\build-win.ps1 -Toolchain msvc-arm64 -BuildDir build-msvc-arm64 # arm64
```

- [ ] Confirm the toolchain data file (`scripts/build/windows-toolchain.psd1`) still
      points at the intended Qt version, FFmpeg component versions and package contents.
- [ ] Verify `dist/MiaCode_<version>_win_x64` and `dist/MiaCode_<version>_win_arm64`.
- [ ] Verify `dist/MiaCode_<version>_win_x64.7z` and `dist/MiaCode_<version>_win_arm64.7z`.
- [ ] Launch the packaged app from `dist/`, not from the build tree.
- [ ] Confirm the arm64 package starts on a machine without the VC++ runtime installed
      (the redistributable DLLs ship inside `app\`).
- [ ] Test opening a sample chart, preview playback, video background decode, SFX playback, and export.

## Checksums

Generate checksums for uploaded artifacts:

```powershell
Get-FileHash .\dist\MiaCode_*_win_x64.7z, .\dist\MiaCode_*_win_arm64.7z -Algorithm SHA256
```

```bash
shasum -a 256 dist/*.zip
```

## License And Notices

- [ ] Review [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md).
- [ ] Confirm package contents match the non-commercial distribution notes.
- [ ] Confirm BASS files are present only in the intended non-commercial release package.
- [ ] Confirm FFmpeg build flags and license notes match the packaged binaries.

## Distribution Channels

[package.yml](../../.github/workflows/package.yml) is the **Nightly** workflow. Branch
pushes and manual runs build Nightly packages. It also exposes the shared build used
by [release.yml](../../.github/workflows/release.yml).

The **Release** workflow is triggered manually against the selected ref. It builds the
same three platforms with `channel: release` and stores the `.7z` packages as Actions
artifacts for 14 days. Website publication uses the OSS procedure below.

Both channels use Release compiler optimization. The About dialog displays the
embedded distribution channel, independently of the compiler configuration and version
prerelease suffix.

Local CMake configurations default to `MIACODE_RELEASE_CHANNEL=nightly`. For a Release
package, configure with `-DMIACODE_RELEASE_CHANNEL=release` and build with
`--config Release`. The packaging entry scripts accept the environment variable
`MIACODE_RELEASE_CHANNEL=release`; Windows standalone packaging reads the channel from
`generated/AppVersion.h`. Nightly package names contain `_nightly`; Release names use
`MiaCode_<version>_<os>_<arch>.7z`.

- [ ] Confirm the selected ref carries the intended version in `CMakeLists.txt`.
- [ ] Add the `## <version>` section to `CHANGELOG.md` for the manifest's `zh_CN` notes.
- [ ] Run **Release** and download its three platform artifacts.
- [ ] Publish the packages and update manifest through the OSS procedure below.

## Update Manifest

The in-app update check reads one fixed URL per major version:

```
https://miacode.oss-cn-beijing.aliyuncs.com/update/<major>/latest.json
```

The update feed is shared by distribution channels. Every build of the major reads
the same manifest and prompts when its version is newer than the client version.
Only Release-channel packages are published to this feed.

The client accepts `schema` 1 and ignores anything higher. It requires `schema`, `major`
and `version` to be whole numbers / a parseable semver, and a `platforms` entry for its
own key that is an object carrying a non-empty `file` and an **https** `url`. Anything
else is treated as "nothing to offer", so a malformed manifest is silently inert rather
than loud. The update dialog shows the notes in-app; its download button opens the
platform `url` in the browser.

```json
{
  "schema": 1,
  "major": 2,
  "version": "2.1.0",
  "releasedAt": "2026-09-20",
  "mandatory": false,
  "notes": { "zh_CN": "...", "en_US": "..." },
  "platforms": {
    "macos-arm64":   { "file": "...7z", "bytes": 0, "sha256": "...", "url": "https://..." },
    "windows-x64":   { "file": "...7z", "bytes": 0, "sha256": "...", "url": "https://..." },
    "windows-arm64": { "file": "...7z", "bytes": 0, "sha256": "...", "url": "https://..." }
  }
}
```

Platform keys must match `.github/workflows/package.yml`'s matrix exactly
(`macos-arm64`, `windows-x64`, `windows-arm64`); a key the client cannot match reads as
"no package for this platform".

### Publishing a version

1. Download the **Release** workflow artifacts, or build locally with
   `MIACODE_RELEASE_CHANNEL=release` (see [Distribution Channels](#distribution-channels)).
   Packages carry the release name `MiaCode_<version>_<os>_<arch>.7z`.
2. Lay out the upload with
   [scripts/release/update-manifest.py](../../scripts/release/update-manifest.py):

   ```bash
   python3 scripts/release/update-manifest.py generate \
     --packages dist --out dist/oss \
     --released-at "$(date -u +%F)" --changelog CHANGELOG.md --allow-partial
   ```

   The version defaults to the one CMakeLists.txt builds. The output mirrors the bucket
   root, and size and hash are computed from the archives copied next to the manifest:

   ```
   dist/oss/release/<version>/MiaCode_<version>_<os>_<arch>.7z
   dist/oss/update/<major>/latest.json
   ```

3. Upload `dist/oss/` to the bucket root, **packages first, manifest last**, so a client
   never reads a manifest whose package is not there yet. Archives under
   `release/<version>/` are never overwritten.
4. Set the HTTP headers of `update/<major>/latest.json` (OSS console → the object →
   设置文件元数据 / file metadata) after **every** upload of it, since an overwrite may
   drop them:
   - `Content-Type`: `application/json`
   - `Cache-Control`: `no-cache`, so a CDN or proxy never serves a stale manifest
   - leave the rest empty. The default OSS domain still answers
     `Content-Disposition: attachment`, which the client ignores.
5. Check the live manifest:

   ```bash
   curl -sI https://miacode.oss-cn-beijing.aliyuncs.com/update/2/latest.json
   ```

   It must answer `200` with `Cache-Control: no-cache`, and the body must match
   `dist/oss/update/<major>/latest.json`.

**The manifest only ever moves forward**, because a client has no way to downgrade:
`generate` reads the live manifest and refuses a version that does not outrank it (pass
`--current-version` to supply it yourself). A missing manifest (HTTP 404) counts as
nothing published; any other read failure stops the run. It also refuses nightly-named
archives, archives of another version, and a manifest past the 64 KiB the client
accepts. Leave out `--allow-partial` when every platform is expected.

`python3 scripts/release/update-manifest.py selfcheck` asserts the version rules against
the same cases as `src/tools/update/UpdateVersionSpec.cpp`; run it after touching either
side, since the C++ and Python comparisons are separate implementations.
