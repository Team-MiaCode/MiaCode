"""Inspect and operate the Android test app using observed accessibility nodes."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument("action", choices=["nodes", "click", "capture"])
parser.add_argument("value", nargs="?")
parser.add_argument("--adb", required=True, type=Path)
parser.add_argument("--serial", default="emulator-5554")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
output = root / "build-devtools/android-ui"
output.mkdir(parents=True, exist_ok=True)
prefix = [str(args.adb), "-s", args.serial]

def call(*command):
    return subprocess.check_output(prefix + list(command), timeout=40)

if args.action == "capture":
    if not args.value:
        parser.error("capture needs an output filename")
    target = (output / args.value).resolve()
    if not target.is_relative_to(output.resolve()):
        parser.error("capture output must be inside build-devtools/android-ui")
    target.write_bytes(call("exec-out", "screencap", "-p"))
    print(target)
else:
    dump = call("shell", "uiautomator", "dump", "/sdcard/miacode-mobile-qa.xml")
    if b"/sdcard/miacode-mobile-qa.xml" not in dump:
        raise RuntimeError("Android accessibility hierarchy unavailable; no click performed")
    xml = call("exec-out", "cat", "/sdcard/miacode-mobile-qa.xml")
    (output / "current-ui.xml").write_bytes(xml)
    nodes = list(ET.fromstring(xml).iter("node"))
    if args.action == "nodes":
        for node in nodes:
            label = node.get("text") or node.get("content-desc")
            if label:
                print(json.dumps({"label": label, "bounds": node.get("bounds"),
                    "package": node.get("package"), "checked": node.get("checked")}, ensure_ascii=True))
    else:
        if not args.value:
            parser.error("click needs an exact observed label")
        matches = []
        for node in nodes:
            if args.value not in (node.get("text"), node.get("content-desc")):
                continue
            bounds = list(map(int, re.findall(r"\d+", node.get("bounds", ""))))
            if len(bounds) == 4 and bounds[2] > bounds[0] and bounds[3] > bounds[1]:
                matches.append(bounds)
        if len(matches) != 1:
            raise RuntimeError(f"Expected one visible target; found {len(matches)} for {args.value!r}")
        x1, y1, x2, y2 = matches[0]
        call("shell", "input", "tap", str((x1 + x2) // 2), str((y1 + y2) // 2))
        print("Clicked", ascii(args.value), "at", (x1 + x2) // 2, (y1 + y2) // 2)
