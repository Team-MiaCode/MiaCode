#!/usr/bin/env python3
"""Lay out a release for manual upload to the MiaCode OSS bucket.

Each major version has one update manifest, update/<major>/latest.json. There
are no channels: whatever is published there is what clients are offered, and
a client only compares "manifest version vs its own version". Nightly
packages are never published as updates.

`generate` writes a directory that mirrors the bucket root:

    <out>/release/<version>/MiaCode_<version>_<os>_<arch>.7z
    <out>/update/<major>/latest.json

Upload its contents to the bucket root as they are. Size and hash come from
the archives themselves, so the manifest cannot disagree with the files next
to it.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import shutil
import sys
import urllib.error
import urllib.request


SCHEMA = 1
# Mirrors manifestUrl() in src/app/services/update/UpdateService.cpp.
OSS_BASE_URL = "https://miacode.oss-cn-beijing.aliyuncs.com"
# SemanticVersion.cpp holds core numbers and numeric prerelease identifiers in
# an int, and refuses anything wider rather than comparing it wrong.
INT_MAX = 2147483647
# Mirrors kMaxPayloadBytes in src/app/services/update/NetworkUpdateFetcher.cpp:
# a manifest past this is refused by every client, so publishing one is worse
# than failing here.
MAX_MANIFEST_BYTES = 64 * 1024
# Release notes are a summary shown in the update dialog, not the changelog.
NOTES_CHAR_LIMIT = 2000
# Release package names are <name>_<version>_<os>_<arch>, so the trailing two
# segments are the only part that says which platform an archive is for.
# The keys must match currentPlatformKey() in
# src/app/services/update/UpdateManifest.cpp and the matrix in
# .github/workflows/package.yml.
PLATFORM_SUFFIXES = {
    "_mac_arm64.7z": "macos-arm64",
    "_win_x64.7z": "windows-x64",
    "_win_arm64.7z": "windows-arm64",
}
PLATFORM_KEYS = ("windows-x64", "windows-arm64", "macos-arm64")

IDENTIFIER = re.compile(r"[0-9A-Za-z-]+")
NUMERIC = re.compile(r"0|[1-9][0-9]*")


def is_digits(text: str) -> bool:
    """ASCII digits only -- str.isdigit() also accepts full-width forms."""
    return bool(text) and all("0" <= character <= "9" for character in text)


def parse_number(text: str):
    """A semver numeric identifier: no leading zeros, and it fits in an int."""
    if not NUMERIC.fullmatch(text):
        return None
    value = int(text)
    return value if value <= INT_MAX else None


def parse_version(text: str):
    """A semver subset mirroring src/app/services/update/SemanticVersion.cpp.

    Returns (major, minor, patch, prerelease_identifiers) or None. Every
    rejection has a matching case in src/tools/update/UpdateVersionSpec.cpp.
    """
    body = text.strip()
    if body[:1] in ("v", "V"):
        body = body[1:]
    # Build metadata takes no part in ordering.
    body = body.split("+", 1)[0]
    core, separator, prerelease_text = body.partition("-")
    if separator and not prerelease_text:
        # "2.0.0-" carries an empty prerelease, which is not the same as none.
        return None
    parts = core.split(".")
    if len(parts) != 3:
        return None
    numbers = []
    for part in parts:
        value = parse_number(part)
        if value is None:
            return None
        numbers.append(value)
    prerelease = []
    if prerelease_text:
        prerelease = prerelease_text.split(".")
        for identifier in prerelease:
            if not IDENTIFIER.fullmatch(identifier):
                return None
            if is_digits(identifier) and parse_number(identifier) is None:
                # A leading zero, or a number the C++ side cannot hold: both
                # would order wrong rather than merely look odd.
                return None
    return (numbers[0], numbers[1], numbers[2], prerelease)


def compare_identifier(left: str, right: str) -> int:
    left_numeric = is_digits(left)
    right_numeric = is_digits(right)
    if left_numeric and right_numeric:
        return (int(left) > int(right)) - (int(left) < int(right))
    if left_numeric != right_numeric:
        return -1 if left_numeric else 1
    return (left > right) - (left < right)


def compare_versions(left, right) -> int:
    """-1 / 0 / +1 over parse_version() tuples."""
    for index in range(3):
        if left[index] != right[index]:
            return -1 if left[index] < right[index] else 1
    left_pre, right_pre = left[3], right[3]
    if bool(left_pre) != bool(right_pre):
        # A release outranks any prerelease of the same core version.
        return 1 if not left_pre else -1
    for left_id, right_id in zip(left_pre, right_pre):
        ordering = compare_identifier(left_id, right_id)
        if ordering != 0:
            return ordering
    if len(left_pre) == len(right_pre):
        return 0
    return -1 if len(left_pre) < len(right_pre) else 1


def strip_tag(tag: str) -> str:
    return tag[1:] if tag[:1] in ("v", "V") else tag


def outranks(parsed, published: str) -> bool:
    """Is `parsed` strictly newer than the version the live manifest names?

    An absent or unreadable baseline counts as "nothing published": a manifest
    somebody hand-edited into an unparseable state must not block the next
    release forever.
    """
    text = (published or "").strip()
    if not text:
        return True
    existing = parse_version(text)
    if existing is None:
        print(f"warning: published version {text!r} does not parse; treating it as unpublished")
        return True
    return compare_versions(parsed, existing) > 0


def platform_for(name: str):
    for suffix, key in PLATFORM_SUFFIXES.items():
        if name.endswith(suffix):
            return key
    return None


def names_version(name: str, version_text: str) -> bool:
    """Is this the release archive of `version_text`?

    A nightly carries a channel segment after the version and is never
    offered as an update, so only the exact release name matches.
    """
    return any(name == f"MiaCode_{version_text}{suffix}" for suffix in PLATFORM_SUFFIXES)


def sha256_of(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def manifest_path(major: int) -> str:
    return f"update/{major}/latest.json"


def release_path(version_text: str, name: str) -> str:
    # One directory per version: a published archive is never overwritten, so
    # a manifest or a download in flight cannot see the bytes change under it.
    return f"release/{version_text}/{name}"


def load_platform_archives(packages_dir: str, version_text: str) -> dict:
    """Key every release archive under `packages_dir` by its platform.

    Archives may sit at any depth, so this walks rather than lists.
    """
    root = pathlib.Path(packages_dir)
    archives = sorted(path for path in root.rglob("*.7z") if path.is_file())
    if not archives:
        raise SystemExit(f"no .7z package found under {root}")
    found: dict = {}
    for archive in archives:
        key = platform_for(archive.name)
        if key is None:
            # Guessing would point a platform at somebody else's download.
            raise SystemExit(f"cannot tell which platform {archive.name} is built for")
        if not names_version(archive.name, version_text):
            # A nightly, or a package of another version. Published, every
            # client would download a package that reports some other version
            # and so keep offering itself the same update forever.
            raise SystemExit(
                f"{archive.name} is not the {version_text} release package -- nightly "
                "packages are not published, and the version must match "
                "MIACODE_VERSION in CMakeLists.txt")
        if key in found:
            raise SystemExit(f"two packages claim {key}: {found[key].name} and {archive.name}")
        found[key] = archive
    return found


def platform_entry(archive: pathlib.Path, version_text: str, base_url: str) -> dict:
    return {
        "file": archive.name,
        "bytes": archive.stat().st_size,
        "sha256": sha256_of(archive),
        "url": f"{base_url}/{release_path(version_text, archive.name)}",
    }


def changelog_notes(path: pathlib.Path, version_text: str) -> dict:
    """Lift the `## <version>` section out of CHANGELOG.md.

    The changelog is written in Chinese, so it can only fill zh_CN; pass
    --notes to supply the other languages.
    """
    if not path.is_file():
        print(f"warning: {path} does not exist; the manifest carries no notes")
        return {}
    body: list[str] = []
    collecting = False
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("## "):
            if collecting:
                break
            collecting = line[3:].strip() == version_text
            continue
        if collecting:
            body.append(line)
    text = "\n".join(body).strip()
    if not text:
        print(f"warning: CHANGELOG.md has no '## {version_text}' section; the manifest carries no notes")
        return {}
    if len(text) > NOTES_CHAR_LIMIT:
        text = text[:NOTES_CHAR_LIMIT].rstrip() + "…"
    return {"zh_CN": text}


def build_manifest(version_text: str, entries: dict, notes: dict, released_at: str) -> dict:
    parsed = parse_version(version_text)
    if parsed is None:
        raise SystemExit(f"not a usable version: {version_text}")
    return {
        "schema": SCHEMA,
        "major": parsed[0],
        "version": version_text,
        "releasedAt": released_at,
        "mandatory": False,
        "notes": notes,
        "platforms": entries,
    }


def published_version(url: str) -> str:
    """The version the live manifest names; empty when none is published.

    Any failure other than "not found" stops the run: treating it as "nothing
    published" would let an older build replace a newer one.
    """
    try:
        with urllib.request.urlopen(url, timeout=20) as response:
            data = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return ""
        raise SystemExit(f"cannot read the published manifest {url}: HTTP {error.code}")
    except (urllib.error.URLError, OSError, ValueError) as error:
        raise SystemExit(f"cannot read the published manifest {url}: {error}")
    version = data.get("version") if isinstance(data, dict) else None
    return version if isinstance(version, str) else ""


def cmake_version(path: pathlib.Path) -> str:
    """The version CMakeLists.txt bakes into the build, e.g. 2.0.0-alpha."""
    text = path.read_text(encoding="utf-8")
    parts = []
    for field in ("MAJOR", "MINOR", "PATCH"):
        found = re.search(rf'set\(MIACODE_VERSION_{field}\s+"([^"]*)"', text)
        if found is None:
            raise SystemExit(f"{path} has no MIACODE_VERSION_{field}")
        parts.append(found.group(1))
    version = ".".join(parts)
    found = re.search(r'set\(MIACODE_VERSION_PRERELEASE\s+"([^"]*)"', text)
    if found is not None and found.group(1):
        version += f"-{found.group(1)}"
    return version


def command_version(args) -> int:
    """Print the version this commit builds, so a rehearsal can name its tag."""
    print(cmake_version(pathlib.Path(args.cmakelists)))
    return 0


def command_verify(args) -> int:
    """Does the tag name the version this commit actually builds?

    Cheap to answer and expensive to get wrong: a tag ahead of CMakeLists.txt
    produces packages that report the old version. Run this before the build,
    not after.
    """
    version_text = strip_tag(args.tag)
    if parse_version(version_text) is None:
        raise SystemExit(f"tag is not a usable version: {args.tag}")
    built = cmake_version(pathlib.Path(args.cmakelists))
    if built != version_text:
        raise SystemExit(
            f"tag {args.tag} builds version {built} -- bump MIACODE_VERSION_* in "
            f"{args.cmakelists} to {version_text}, or retag")
    print(f"{args.tag} matches the {built} this commit builds")
    return 0


def command_generate(args) -> int:
    version_text = strip_tag(args.version) if args.version else cmake_version(pathlib.Path(args.cmakelists))
    parsed = parse_version(version_text)
    if parsed is None:
        raise SystemExit(f"not a usable version: {version_text}")
    base_url = args.base_url.rstrip("/")

    # A client cannot downgrade, so the manifest only ever moves forward.
    if args.current_version is None:
        current = published_version(f"{base_url}/{manifest_path(parsed[0])}")
    else:
        current = args.current_version
    print(f"published: {current or 'nothing'}")
    if not outranks(parsed, current):
        raise SystemExit(f"{version_text} does not outrank the published {current}; nothing written")

    archives = load_platform_archives(args.packages, version_text)
    missing = [key for key in PLATFORM_KEYS if key not in archives]
    if missing:
        if not args.allow_partial:
            raise SystemExit(
                "no package for " + ", ".join(missing)
                + " -- those clients would see no update at all. Pass --allow-partial if that is intended.")
        print(f"warning: publishing without {', '.join(missing)}")

    if args.notes:
        notes = json.loads(pathlib.Path(args.notes).read_text(encoding="utf-8"))
    elif args.changelog:
        notes = changelog_notes(pathlib.Path(args.changelog), version_text)
    else:
        notes = {}

    entries = {key: platform_entry(archive, version_text, base_url) for key, archive in archives.items()}
    manifest = build_manifest(version_text, entries, notes, args.released_at)
    encoded = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if len(encoded) > MAX_MANIFEST_BYTES:
        raise SystemExit(
            f"the manifest is {len(encoded)} bytes, past the {MAX_MANIFEST_BYTES} "
            "the client accepts -- shorten the release notes")

    output_dir = pathlib.Path(args.out)
    written = []
    for archive in archives.values():
        target = output_dir / release_path(version_text, archive.name)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(archive, target)
        written.append(str(target))
    path = output_dir / manifest_path(parsed[0])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)
    written.append(str(path))

    print(json.dumps({"written": written}, ensure_ascii=False))
    return 0


def command_selfcheck(_args) -> int:
    """Assertions mirroring src/tools/update/UpdateVersionSpec.cpp."""
    failures = []

    def check(condition, message):
        if not condition:
            failures.append(message)

    # ---- Parsing ----
    check(parse_version("2.1.0") == (2, 1, 0, []), "a release version parses with an empty prerelease")
    check(parse_version("2.0.0-beta.3") == (2, 0, 0, ["beta", "3"]), "prerelease identifiers split on dots")
    check(parse_version("v2.0.0-alpha") == (2, 0, 0, ["alpha"]), "a leading v is accepted so release tags parse")
    check(parse_version("2.0.0-alpha+20260915") == (2, 0, 0, ["alpha"]), "build metadata is discarded")
    check(parse_version("V2.0.0") == (2, 0, 0, []), "an uppercase leading V is accepted just like lowercase v")
    check(parse_version("2.0.0-2147483647") == (2, 0, 0, ["2147483647"]), "a numeric identifier at INT_MAX still parses")
    check(parse_version("2.0.0-0") == (2, 0, 0, ["0"]), "a bare zero numeric identifier is legal")

    for bad in ("", "2.0", "2.0.0.1", "2.0.x", "2.0.0-", "2.0.0-beta..1", "-1.0.0", "nightly",
                "2.0.0-99999999999999999999", "2.0.0-2147483648", "2.01.0", "2.0.0-01", "2.0.0-０"):
        check(parse_version(bad) is None, f"malformed version is rejected: {bad!r}")

    # ---- Ordering ----
    for left, right, expected in (
        ("2.1.0", "2.0.0", 1),
        ("2.0.10", "2.0.9", 1),
        ("10.0.0", "9.9.9", 1),
        ("2.0.0", "2.0.0", 0),
        ("2.0.0-alpha", "2.0.0", -1),
        ("2.0.0", "2.0.0-alpha", 1),
        ("2.0.0-alpha", "1.9.9", 1),
        ("2.0.0-alpha", "2.0.0-beta.1", -1),
        ("2.0.0-beta.2", "2.0.0-beta.10", -1),
        ("2.0.0-beta.1", "2.0.0-beta", 1),
        ("2.0.0-1", "2.0.0-alpha", -1),
        ("2.0.0-beta.1", "2.0.0-alpha", 1),
        ("2.0.0-alpha", "2.0.0-alpha", 0),
    ):
        actual = compare_versions(parse_version(left), parse_version(right))
        check(actual == expected, f"compare({left}, {right}) = {actual}, expected {expected}")

    # ---- Publishing only moves forward ----
    for version, published, expected in (
        ("2.1.0", "", True),
        ("2.1.0", "2.0.0", True),
        ("2.0.0", "2.0.0-alpha", True),
        ("2.0.0-alpha", "2.0.0-alpha", False),
        ("2.0.0-alpha", "2.0.0", False),
        ("2.1.0", "garbage", True),
    ):
        actual = outranks(parse_version(version), published)
        check(actual == expected, f"outranks({version}, {published!r}) = {actual}, expected {expected}")

    # ---- Package naming ----
    for name, expected in (
        ("MiaCode_2.1.0_mac_arm64.7z", "macos-arm64"),
        ("MiaCode_2.1.0_win_x64.7z", "windows-x64"),
        ("MiaCode_2.1.0_win_arm64.7z", "windows-arm64"),
        ("MiaCode_2.1.0-beta.3_mac_arm64.7z", "macos-arm64"),
        ("MiaCode_2.1.0_nightly_win_x64.7z", "windows-x64"),
        ("MiaCode_2.1.0_linux_x64.7z", None),
        ("MiaCode_2.1.0_mac_arm64.zip", None),
    ):
        actual = platform_for(name)
        check(actual == expected, f"platform_for({name}) = {actual}, expected {expected}")

    for name, version_text, expected in (
        ("MiaCode_2.1.0_mac_arm64.7z", "2.1.0", True),
        ("MiaCode_2.0.0-alpha_win_x64.7z", "2.0.0-alpha", True),
        # Nightlies are never published as updates.
        ("MiaCode_2.1.0_nightly_win_x64.7z", "2.1.0", False),
        ("MiaCode_2.0.0-alpha_mac_arm64.7z", "2.1.0", False),
        ("MiaCode_2.1.0-beta.1_mac_arm64.7z", "2.1.0", False),
    ):
        actual = names_version(name, version_text)
        check(actual == expected, f"names_version({name}, {version_text}) = {actual}, expected {expected}")

    # ---- Manifest shape and bucket layout ----
    entries = {"macos-arm64": {"file": "a.7z", "bytes": 1, "sha256": "x", "url": "https://x"}}
    release = build_manifest("2.1.0", entries, {}, "2026-09-20")
    check(release["major"] == 2 and release["version"] == "2.1.0", "build_manifest lost the version")
    check(release["schema"] == SCHEMA, "build_manifest wrote the wrong schema")
    check(set(release) == {"schema", "major", "version", "releasedAt", "mandatory", "notes", "platforms"},
          "build_manifest writes exactly the fields the client reads")
    check(manifest_path(2) == "update/2/latest.json", "the manifest path must match manifestUrl()")
    check(release_path("2.1.0", "a.7z") == "release/2.1.0/a.7z", "packages live under release/<version>/")

    if failures:
        for line in failures:
            print(f"FAIL: {line}", file=sys.stderr)
        return 1
    print("update_manifest_script ok")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    generate = sub.add_parser("generate", help="lay out the manifest and packages for upload to OSS")
    generate.add_argument("--packages", required=True,
                          help="directory holding the release .7z archives, at any depth")
    generate.add_argument("--out", required=True, help="output directory, mirroring the bucket root")
    generate.add_argument("--released-at", required=True, help="YYYY-MM-DD")
    generate.add_argument("--version", default="",
                          help="version to publish; defaults to the one CMakeLists.txt builds")
    generate.add_argument("--cmakelists", default="CMakeLists.txt", help="path to CMakeLists.txt")
    generate.add_argument("--base-url", default=OSS_BASE_URL, help="public URL of the bucket root")
    generate.add_argument("--current-version", default=None,
                          help="version the live manifest names ('' for none); read from --base-url when omitted")
    generate.add_argument("--notes", help="path to a JSON object of language token -> notes")
    generate.add_argument("--changelog", help="CHANGELOG.md to lift the zh_CN notes from")
    generate.add_argument("--allow-partial", action="store_true",
                          help="publish even though some platform has no package")
    generate.set_defaults(func=command_generate)

    version = sub.add_parser("version", help="print the version CMakeLists.txt builds")
    version.add_argument("--cmakelists", default="CMakeLists.txt", help="path to CMakeLists.txt")
    version.set_defaults(func=command_version)

    verify = sub.add_parser("verify", help="check the tag against the version the build bakes in")
    verify.add_argument("--tag", required=True, help="release tag, e.g. v2.1.0")
    verify.add_argument("--cmakelists", default="CMakeLists.txt", help="path to CMakeLists.txt")
    verify.set_defaults(func=command_verify)

    selfcheck = sub.add_parser("selfcheck", help="run the built-in assertions")
    selfcheck.set_defaults(func=command_selfcheck)

    args = parser.parse_args()
    result = args.func(args)
    sys.exit(result if isinstance(result, int) else 0)


if __name__ == "__main__":
    main()
