# Offline browser release

This release includes solo practice, matches against Ernie at three paces,
all 34 weapons, shopping/refunds, reconnaissance, keyboard and touch buttons,
pause/resume, restart, and match results. Gameplay runs locally in WebAssembly.
The offline page has no accounts, remote opponents, rankings, or saved matches. Reloading
starts a new match. The first page load needs HTTP(S); after loading, play does
not depend on a game server. This is not an installable/offline-cached PWA.

The separate `online.html` client supports private human matches when connected
to the room service. See [multiplayer setup](../MULTIPLAYER.md). A static host
alone cannot provide online matches.

Sound is intentionally absent. The [sound inventory](audio/README.md) lists
47 empty cues and all 34 weapon mappings so recordings can be added later.
The original default Gimp artwork is included. No generated replacement sounds
or missing-file requests are used as placeholders.

## Build and package

Activate Emscripten 4.0.15, and install a native C++ compiler, Node.js, and Python 3.
From the repository root:

```sh
make -C web test test-wasm check-dist archive
python3 web/audio/check.py
make -C web serve
```

The development game is at `http://127.0.0.1:8000/`. Release files are in
`web/dist/`, with a ZIP at `web/build/battletris-browser.zip`. The package includes
the MIT license, a SHA-256 asset manifest, and deployment instructions. The ZIP
is reproducible from the same build inputs; compiler upgrades can change output.

Serve `web/dist/` with a static HTTP(S) host, including beneath a subpath. Serve
WASM as `application/wasm`. Revalidate `index.html` and `manifest.json`; immutable
content-hashed assets may be cached for a year. The included `_headers` file is
for hosts that understand that format. Configure equivalent rules elsewhere.
Upload new hashed assets before replacing the HTML, and retain previous assets
during rollout. No credentials, server API, or cross-origin isolation is needed.

The page offers Retry if scripts, WASM, or artwork cannot load, and reports a
startup timeout after 30 seconds. JavaScript-disabled browsers get an explanation.

## Verification

The checked-in GitHub Actions workflow runs portable sanitizer tests, WASM
checks, native/WASM replay comparisons, packaging checks, browser checks against
both development and packaged subpaths, and the original Motif smoke test.
It has been linted locally; hosted CI must run in the repository to establish its
remote result. It builds downloadable artifacts but does not deploy anything.

Install Playwright outside the repository and run the browser suite:

```sh
npm install --prefix /tmp/battletris-ui-checks playwright@1.62.1
node /tmp/battletris-ui-checks/node_modules/playwright/cli.js install chromium firefox webkit
NODE_PATH=/tmp/battletris-ui-checks/node_modules node web/tests/browsers.cjs
```

For a packaged site running on another URL:

```sh
python3 web/tests/http_dist.py http://127.0.0.1:8001/dist/
BASE_URL=http://127.0.0.1:8001/dist/ NODE_PATH=/tmp/battletris-ui-checks/node_modules node web/tests/browsers.cjs
```

Native and WASM tests cover all weapons and targeted interactions, including
recon activation/expiry, terminal cleanup, timer restart boundaries, and bazaar
freezing. Replay checks compare 24 seeded traces and 720 cumulative checkpoints.
Browser checks exercise keyboard and touch controls, focus loss, shopping with
earned funds, refunds/launches, complete loss/restart, responsive layouts, and
recovery from missing scripts, WASM, and artwork. Screenshots are written to
`.playwright-mcp/` for inspection.

## Compatibility and scope decisions

The local test platforms are Chromium 151.0.7922.34, Firefox 153.0, and
Playwright WebKit 26.5 on macOS. Touch input is emulated. Physical iPhone/iPad
and Android devices, and branded Safari, have not been tested. Before calling
those platforms supported, check a full match, both orientations, focus loss,
shopping, scrolling, and reload recovery on the actual device. WebKit testing
alone is not a Safari/device certification.

The portable core preserves original piece probabilities, shapes, scoring,
weapon prices/durations, and the native Ernie evaluator. It intentionally uses
independent deterministic random streams, a bounded reachable-placement planner,
visible AI movement, three selected paces, simplified shopping/attack strategy,
and immediate top-out detection. Some disruptive weapons affect browser Ernie
even when native Ernie was exempt. Soft/hard drop and browser keyboard repeat
are adapted controls. These are release scope decisions, not promises of exact
native gameplay or AI strategy parity. See [PORTING.md](../PORTING.md) for detail.

The native Motif game builds and passes its Ernie smoke check in Debian 12 ARM64
with GCC 12.2 and Motif 2.3.8. The macOS Motif target and exhaustive native GUI
interaction comparisons remain unverified. Core sanitizer/replay checks compare
the portable build to WASM, not the entire original application.
