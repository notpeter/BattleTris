#!/usr/bin/env python3
"""Verify distribution integrity, cache invalidation and reproducible archives."""

import hashlib
import importlib.util
import json
import re
from pathlib import Path
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("battletris_package", ROOT / "package.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


def verify(directory):
    for document in ("RELEASE.md", "PORTING.md", "audio/README.md"):
        content = (directory / document).read_text()
        for target in re.findall(r"\]\(([^)]+)\)", content):
            if "://" not in target and not target.startswith("#"):
                assert (directory / document).parent.joinpath(target).is_file(), (document, target)
    manifest = json.loads((directory / "manifest.json").read_text())
    for item in manifest["assets"].values():
        data = (directory / item["file"]).read_bytes()
        assert hashlib.sha256(data).hexdigest() == item["sha256"], item["file"]
        assert len(data) == item["bytes"], item["file"]
    assets = manifest["assets"]
    html = (directory / "index.html").read_text()
    assert 'src="' + assets["battletris.js"]["file"] + '"' in html
    assert 'src="' + assets["app.js"]["file"] + '"' in html
    if "online.html" in assets:
        online = (directory / "online.html").read_text()
        assert 'src="' + assets["online.js"]["file"] + '"' in online
        assert 'src="online.js"' not in online
    loader = (directory / assets["battletris.js"]["file"]).read_text()
    assert json.dumps(assets["battletris.wasm"]["file"]) in loader
    assert '"battletris.wasm"' not in loader
    assert not list(directory.glob("*test*"))
    return manifest


verify(ROOT / "dist")
with tempfile.TemporaryDirectory(prefix="battletris-package-test-") as temporary:
    base = Path(temporary)
    source, output = base / "source", base / "dist"
    source.mkdir()
    (source / "index.html").write_text('<script src="battletris.js"></script><script src="app.js"></script>')
    (source / "battletris.js").write_text('locateFile("battletris.wasm")')
    (source / "battletris.wasm").write_bytes(b"first binary")
    (source / "online.html").write_text('<script src="online.js"></script>')
    (source / "online.js").write_text('draw("assets/icon.svg")')
    (source / "app.js").write_text('startGame("assets/icon.svg")')
    (source / "assets").mkdir()
    (source / "assets" / "icon.svg").write_text("<svg/>")
    first = packager.package(source, output)
    verify(output)
    first_zip = packager.archive(output).read_bytes()
    assert packager.package(source, output) == first
    assert packager.archive(output).read_bytes() == first_zip

    # A WASM-only update must invalidate BOTH binary and loader, while the
    # app's hash stays unchanged. The next HTML points at the new loader.
    (source / "battletris.wasm").write_bytes(b"second binary")
    second = packager.package(source, output)
    verify(output)
    assert second["release"] != first["release"]
    for key in ["battletris.wasm", "battletris.js"]:
        assert second["assets"][key]["file"] != first["assets"][key]["file"]
    assert second["assets"]["app.js"] == first["assets"]["app.js"]
    assert not (output / first["assets"]["battletris.js"]["file"]).exists()

    # Artwork-only changes propagate into the app and HTML's script URL.
    (source / "assets" / "icon.svg").write_text('<svg width="10"/>')
    third = packager.package(source, output)
    verify(output)
    assert third["assets"]["assets/icon.svg"] != second["assets"]["assets/icon.svg"]
    assert third["assets"]["app.js"] != second["assets"]["app.js"]
    assert third["assets"]["online.js"] != second["assets"]["online.js"]
    assert third["assets"]["battletris.js"] == second["assets"]["battletris.js"]
    assert third["assets"]["assets/icon.svg"]["file"] in (output / third["assets"]["app.js"]["file"]).read_text()

    # A failed package does not replace the previous usable distribution.
    (source / "battletris.wasm").unlink()
    try:
        packager.package(source, output)
        raise AssertionError("Missing WASM should fail")
    except FileNotFoundError:
        pass
    assert verify(output) == third
print("Distribution integrity, WASM cache invalidation, and reproducible archive checks passed")
