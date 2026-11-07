#!/usr/bin/env python3
"""Select an installed Apple SDK that preserves legacy CoreMedia C linkage."""
import json
import re
import subprocess
import sys
from pathlib import Path


def supports_legacy_coremedia(sdk):
    stub = sdk / "System/Library/Frameworks/CoreMedia.framework/CoreMedia.tbd"
    if not stub.is_file():
        return False
    # SDK 27 redirects the whole framework, not just its Swift symbols, for
    # targets before 10.14.4. The extension uses C APIs and targets Intel 10.13.
    redirect = r"\$ld\$previous\$@rpath/libswiftCoreMedia\.dylib\$\$1\$[^$]+\$[^$]+\$\$"
    return not re.search(redirect, stub.read_text())


def sdk_version(sdk):
    try:
        data = json.loads((sdk / "SDKSettings.json").read_text())
        return tuple(int(part) for part in data["Version"].split("."))
    except (OSError, ValueError, KeyError):
        return ()


default = Path(subprocess.check_output(
    ["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True).strip())
candidates = [default]
alternatives = list(Path("/Library/Developer/CommandLineTools/SDKs").glob("MacOSX*.sdk"))
alternatives += list(Path("/Applications").glob(
    "Xcode*.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX*.sdk"))
candidates += sorted(alternatives, key=sdk_version, reverse=True)
seen = set()
for sdk in candidates:
    sdk = sdk.resolve()
    if sdk in seen:
        continue
    seen.add(sdk)
    if supports_legacy_coremedia(sdk):
        print(sdk)
        sys.exit(0)
sys.exit("No installed SDK preserves CoreMedia linkage for Intel macOS 10.13. "
         "Install/select the macOS 26.5 SDK or an earlier compatible SDK.")
