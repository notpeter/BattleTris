# BattleTris ports

BattleTris supports the original X11/Motif application and a browser frontend
built from the reusable C++ rules. Browser play includes solo practice, Ernie,
and private two-human matches with all 34 weapons. See the
[gameplay guide](web/guide.html), [browser release guide](web/RELEASE.md), and
[multiplayer setup](MULTIPLAYER.md).

## Architecture

The browser compiles the rules with Emscripten and replaces X11 drawing, Xt
timers, and native networking. Running Motif itself through an X server in the
browser would retain more of the old UI but add a large compatibility layer.
Streaming the native desktop would need a server per session. Reusing the rules
keeps offline play local and allows an ordinary Canvas/DOM interface.

- `usr/src/game/BTPiece.C`, `BTPieceManager.C`, and `BTBoardManager.C` retain
  shapes, weighted selection, collision, line clearing, dice, and smiles.
- `usr/src/portable/BTBoxPortable` supplies headless boxes with the original
  IDs and values. `BT_PORTABLE` selects this boundary without removing Motif.
- `web/Game` owns a board, weapons, scoring, and explicit timers. `web/Match`
  coordinates two boards, combat, shopping, results, and Ernie scheduling.
- `usr/src/portable/BTPlanner` searches reachable placements and uses the
  original `BTCBoard` evaluator. All 15 native Ernie speed levels are available,
  from Comatose through Bionic. It does not run the native Xt controller.
- `web/Recon` maintains seeded, cached spy reports. Reading a report does not
  change its noise or advance any game random stream.
- `web/exports.C` exposes the small C API used by both the browser and Node.
- `web/ui.js` and `style.css` share board rendering, the bazaar, and responsive
  presentation between the offline and online clients. Colors and tile geometry
  follow `BattleTris.C` and `BTBox.C`; board, score, and arsenal ordering follows the native game.
  Boards snap to whole device-pixel cells, with pip borders rendered at the
  display resolution. Space starts native-style fast descent and retains the
  150 ms landing window.
- The weapon catalog is generated from `usr/src/share/btweapons*.db`.
- `server/index.cjs` runs one authoritative WASM instance per private room.
  Native daemons, player databases, and ranking are separate services and are
  not compiled into the page.

## Build the browser

Use Emscripten 4.0.15, a native C++ compiler, Python 3, and Node.js 24 or newer.
With the Emscripten environment active, run from the repository root:

```sh
make -C web all
make -C web serve
```

Open `http://127.0.0.1:8000/`. The board opens directly; select Play to begin. Match settings
select solo practice or an Ernie pace for the next restart. Online play requires
the room service described in [MULTIPLAYER.md](MULTIPLAYER.md).

```sh
make -C web test test-wasm check-dist archive
python3 web/audio/check.py
```

`web/dist` contains the static release; `web/build/battletris-browser.zip` is
its reproducible archive. The package hashes scripts, styles, WASM, and artwork.
Serve it over HTTP(S), with `.wasm` as `application/wasm`. A static host supports
solo and Ernie; private multiplayer additionally requires the same-origin `/ws`
backend. See [web/RELEASE.md](web/RELEASE.md) for cache and rollout details.

## Build the native application on macOS

Install Xcode Command Line Tools, XQuartz, and Homebrew OpenMotif. Build against
Homebrew's matching X11 client libraries; XQuartz supplies the display server.
The commands work with either Homebrew prefix:

```sh
brew install openmotif
cd usr/src
brew_prefix=$(brew --prefix)
motif_prefix=$(brew --prefix openmotif)
./configure --with-motif-includes="$motif_prefix/include" \
  --with-motif-libs="$motif_prefix/lib" \
  --x-includes="$brew_prefix/include" --x-libraries="$brew_prefix/lib"
make -j4
make check
```

Start XQuartz, then run from an XQuartz terminal:

```sh
./game/BattleTris -X -m
```

`-X` disables the player database/network manager so Ernie needs no daemon.
`-m` mutes the missing sound assets. The configured paths find artwork and
weapon databases directly in the source tree; installation is not required.
The build produces the client, `btserverd`, `btslaved`, and `btref`. The ARM64
client was compiled with Apple Clang and Homebrew OpenMotif and exercised through
startup, challenge, and Ernie play in XQuartz's virtual X server.
Arrow keys move/rotate, Down steps down, and Space drops. J/K/L still provide
left/rotate/right; P pauses and C toggles Condor. Default key translations are
converted during Xt resource loading, after toolkit initialization.

Configure's install helper lives in tracked `usr/src/build-aux`, separate from
the generated `usr/bin` directory. Both generated configure and its source are
kept in sync; autoconf is not needed for a normal build. `make check` exercises
configuration ownership, copying, repeated keys, and invalid paths under address
and undefined-behavior sanitizers. The daemon PID path reserves both its slash
and terminator.

## Native network play

`-X` intentionally disables the native server connection. Start the native
server on one machine (from the repository root):

```sh
./usr/src/daemons/btserverd -f "$PWD/usr/src/daemons/btserver.cf" -n 2
```

It creates its player/network databases and logs in the directories named in
`btserver.cf` and listens on TCP port 4404. Configure generates the file with
paths for this checkout. Two macOS clients were verified connecting to an
isolated server with freshly created databases.

On each player's XQuartz terminal, launch without `-X`:

```sh
./usr/src/game/BattleTris -m -S SERVER_LAN_IP
```

Use `127.0.0.1` only when the server runs on that same computer. Open Challenge,
select the other player, and send the challenge; the other player accepts.
Both players need direct connectivity to each other as well as the server:
the roster server introduces peers, but does not relay matches. Use the same
LAN or a VPN that permits peer connections. Allow `BattleTris` incoming
connections in macOS's firewall; clients listen on dynamically assigned TCP
ports. A custom server port uses `btserverd -p PORT` and `BattleTris -P PORT`.
The native protocol is separate from the browser's Node/WebSocket server.

## Build the native application on Linux

Install a C++ compiler, make, Motif, Xt, X11, and Xext development packages.
On Debian/Ubuntu:

```sh
sudo apt-get install build-essential libmotif-dev libxt-dev libxext-dev libx11-dev
cd usr/src
./configure
make
./game/BattleTris -X -m
```

The repeatable GUI smoke check uses a non-root player, Xvfb, local native
services, and fixed coordinates. It starts Ernie, checks a frozen pause, and
checks that resume changes the display:

```sh
podman build -t localhost/battletris-native-check -f tests/native/Containerfile .
podman run --name battletris-native-smoke localhost/battletris-native-check
podman cp battletris-native-smoke:/tmp/native-results ./native-results
podman rm battletris-native-smoke
```

Docker accepts the same commands. No ports need to be published.

## Compatibility decisions

- Original piece probabilities, scoring, prices, durations, and the placement
  evaluator are retained. The browser uses explicit seeded 32-bit PRNG streams
  instead of platform-specific libc randomness. Players have independent
  streams, not identical pieces; AI search and drawing cannot consume them.
- The server orders human inputs and advances both boards in 10 ms steps.
  Both clients receive views of that simulation. Seed plus ordered inputs and
  ticks reproduces random pieces, attacks, and reconnaissance.
- Ernie uses bounded reachable-path search, visible movement, three selected
  paces, simplified buying/attack tactics, and immediate failed-spawn detection.
  Native Ernie takes whole-piece turns and has exemptions from some disruptive
  effects. These differences are intentional adaptations, not exact AI parity.
- The browser maps arrows, Space, P, and numbered arsenal slots to keyboard
  controls and provides touch buttons. Small screens reflow the desktop layout.
- A blocked descent allows 150 ms to move or rotate without resetting the
  deadline. Slide Denied removes that grace. Hard drop locks immediately.
  Hatter and Slick operate every 20 ms; manual down stops Slick for that piece.
- Upbyside-down reverses gravity and horizontal controls, while rotation remains
  unchanged. Swap cancels inversion and Bottleneck on both boards. Fallout
  allows pieces to leave through the six-column floor gap without rewards.
- Attacks arrive at piece boundaries. The bazaar opens every 20 combined lines;
  both players must finish shopping. Pause and shopping freeze game timers.
- Ernie starts with the native free Condor behavior. Its initial request is
  queued: the first opponent placement activates it; a later placement reports
  settled cells. C disables/re-enables it. Human matches must buy spies.
- Spies never reveal falling pieces, arsenals, or invisible blocks. Ames reports
  roughly half the visible cells with noisy funds; Ace reports about 85 percent
  with occasional funds noise; Condor is accurate. Reports expire with the spy.
- Terminal cleanup removes active pieces, queued attacks, and reconnaissance;
  the browser retains settled boards, scores, and results until restart.
- Sound remains absent. [Sound placeholders](web/audio/README.md) inventory the
  missing cues. Native audio code remains intact.

## Verification and remaining work

The core tests run with native address/undefined-behavior sanitizers and WASM
heap/stack checks. Native/WASM replay compares 48 seeded Ernie/human combat
traces at 1440 cumulative checkpoints, including both boards, random state,
effects, inventory, and reconnaissance. This compares portable builds, not the
entire historical Motif application.

Browser tests cover keyboard/touch input, focus and pause, earned-funds shopping,
refunds, attacks, results/restart, responsive layouts, and missing-file recovery.
Desktop checks cover page/sidebar overflow and Condor visibility before, during,
and after reconnaissance reports.
The server suite covers private snapshots, command validation, sequence replay,
pause/reconnect limits, and checkpoint reconstruction. Packaging checks cover
hashes, cache invalidation, local references, guide prices, and reproducibility.
See the release and multiplayer guides for commands and test limitations.

Remaining release work is real-device and separate-network playtesting and,
if desired, HTTPS hosting. Prediction, durable matches, accounts, matchmaking,
rankings, spectators, and native protocol interoperability are separate projects.
