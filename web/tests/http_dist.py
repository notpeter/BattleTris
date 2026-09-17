#!/usr/bin/env python3
"""Check an HTTP-served distribution against its local build manifest."""

import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urljoin
from urllib.request import urlopen

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("url", help="Distribution URL, including any subpath")
options = parser.parse_args()
base = options.url.rstrip("/") + "/"
local = json.loads((Path(__file__).resolve().parents[1] / "dist" / "manifest.json").read_text())
with urlopen(urljoin(base, "manifest.json"), timeout=10) as response:
    assert json.load(response) == local, "Server manifest differs from local release"
for original, item in local["assets"].items():
    with urlopen(urljoin(base, item["file"]), timeout=10) as response:
        data = response.read()
        assert hashlib.sha256(data).hexdigest() == item["sha256"], item["file"]
        assert len(data) == item["bytes"], item["file"]
        if original.endswith(".wasm"):
            assert response.headers.get_content_type() == "application/wasm", "Wrong WASM MIME type"
        if original.endswith(".css"):
            assert response.headers.get_content_type() == "text/css", "Wrong CSS MIME type"
        if original.endswith(".js"):
            assert response.headers.get_content_type() in ("text/javascript", "application/javascript"), "Wrong JS MIME type"
print("HTTP release " + local["release"] + ": all assets match manifest and script/WASM MIME types are correct")
