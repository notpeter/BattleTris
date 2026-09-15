#!/usr/bin/env python3
"""Package a static release; generated files stay out of the development build."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent


def digest(data):
    return hashlib.sha256(data).hexdigest()


def package(source, target):
    # Build the complete release first, so a missing dependency cannot remove
    # the last usable distribution. Deliberately exclude test/debug executables.
    with tempfile.TemporaryDirectory(prefix="battletris-package-") as temporary:
        stage = Path(temporary)
        assets = {}

        def emit(name, data, hashed=True):
            original = Path(name)
            filename = (original.with_name(original.stem + "." + digest(data)[:16]
                                           + original.suffix).as_posix() if hashed else name)
            destination = stage / filename
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
            assets[name] = {"file": filename, "sha256": digest(data), "bytes": len(data)}
            return filename

        wasm = emit("battletris.wasm", (source / "battletris.wasm").read_bytes())
        loader = (source / "battletris.js").read_text()
        # Emscripten's loader must request the matching hashed binary, including
        # when the site is served beneath a path such as /games/battletris/.
        if loader.count('"battletris.wasm"') != 1:
            raise ValueError("Expected one Emscripten battletris.wasm filename; inspect the generated loader")
        loader = loader.replace('"battletris.wasm"', json.dumps(wasm))
        emit("battletris.js", loader.encode())

        asset_root = source / "assets"
        if asset_root.exists():
            for asset in sorted(asset_root.rglob("*")):
                if asset.is_file():
                    emit(asset.relative_to(source).as_posix(), asset.read_bytes())

        def asset_references(text):
            for name, asset in assets.items():
                if name.startswith("assets/"):
                    text = text.replace(name, asset["file"])
            return text

        html = asset_references((source / "index.html").read_text())
        scripts = re.findall(r'<script\b[^>]*\bsrc="([^"?#]+\.js)"', html)
        if "battletris.js" not in scripts or "app.js" not in scripts:
            raise ValueError("Missing expected browser entry scripts")
        for script in scripts:
            if Path(script).name != script:
                raise ValueError("Entry scripts must be local files: " + script)
            if script not in assets:
                emit(script, asset_references((source / script).read_text()).encode())
            html = html.replace('src="' + script + '"', 'src="' + assets[script]["file"] + '"')
        emit("index.html", html.encode(), hashed=False)

        emit("LICENSE", (ROOT.parent / "LICENSE").read_bytes(), hashed=False)
        if (ROOT / "RELEASE.md").exists():
            release_notes = (ROOT / "RELEASE.md").read_text().replace("(../PORTING.md)", "(PORTING.md)")
            emit("RELEASE.md", release_notes.encode(), hashed=False)
        porting = (ROOT.parent / "PORTING.md").read_text().replace("(web/", "(")
        porting = porting.replace("[`BTGame::exposeEvent`](usr/src/game/BTGame.C)", "`BTGame::exposeEvent`")
        emit("PORTING.md", porting.encode(), hashed=False)
        emit("MULTIPLAYER.md", (ROOT.parent / "MULTIPLAYER.md").read_bytes(), hashed=False)
        for name in ("README.md", "manifest.json"):
            emit("audio/" + name, (ROOT / "audio" / name).read_bytes(), hashed=False)

        release = digest(json.dumps(assets, sort_keys=True).encode())[:16]
        manifest = {"format": 1, "release": release, "assets": assets}
        (stage / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        # Netlify/Cloudflare Pages syntax. Other servers must apply the same
        # policy themselves; most generic static servers ignore this file.
        # Use disjoint paths: some hosts concatenate matching headers rather
        # than overriding a wildcard with the more specific rule.
        headers = "/\n  Cache-Control: no-cache\n/manifest.json\n  Cache-Control: no-cache\n"
        for name, asset in assets.items():
            policy = "public, max-age=31536000, immutable" if asset["file"] != name else "no-cache"
            headers += "/" + asset["file"] + "\n  Cache-Control: " + policy + "\n"
        (stage / "_headers").write_text(headers)
        (stage / "DEPLOYMENT.txt").write_text(
            "BattleTris static browser release " + release + "\n\n"
            "Serve this directory over HTTP(S); file:// is unsupported.\n"
            "Serve .wasm as application/wasm and .js as text/javascript.\n"
            "Revalidate index.html, manifest.json, and stable assets on each visit.\n"
            "Content-hashed files may use max-age=31536000, immutable.\n"
            "The _headers file applies this policy on hosts that support it;\n"
            "configure equivalent rules on other hosts (including subpath prefixes).\n"
            "Upload hashed assets before replacing index.html. Keep previous hashed\n"
            "assets during rollout so already-open pages can finish loading.\n"
            "No server API, account, service worker, or cross-origin isolation is required.\n"
            "All gameplay runs locally after loading. A first visit still needs a server.\n")
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(stage, target)
    return manifest


def archive(target):
    destination = target.parent / "build" / "battletris-browser.zip"
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED) as output:
        for item in sorted(target.rglob("*")):
            if item.is_file():
                info = zipfile.ZipInfo(item.relative_to(target).as_posix(), (1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                output.writestr(info, item.read_bytes())
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", action="store_true")
    options = parser.parse_args()
    result = package(ROOT / "build", ROOT / "dist")
    print("Static release " + result["release"] + " ready in web/dist")
    if options.archive:
        print(archive(ROOT / "dist"))
