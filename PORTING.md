# BattleTris Porting Notes

BattleTris can run locally in a browser by compiling its reusable C++ game rules
with Emscripten and replacing its X11/Motif presentation and Xt scheduling. This
is a frontend port, not a configure flag that makes the entire Unix application
browser-compatible. Keep the native Motif application as a supported target.

The implementation under `web/` supports solo practice and an offline match
against Ernie. It exercises original piece, board, and AI evaluation code through
a small C API and a Canvas/DOM frontend. Ernie's search and scheduling are adapted
for the browser. Offline matches include a bazaar and all 34 weapons from the
original catalogue, including reconnaissance. The offline release scope and
validation limits are recorded in [web/RELEASE.md](web/RELEASE.md). Native AI
strategy is intentionally adapted; this release is silent and excludes
networking and rankings. Sound cues are inventoried in
[web/audio/manifest.json](web/audio/manifest.json) for later recordings.

## What the Codebase Actually Couples

Source paths below are relative to `usr/src/`.

- `game/BTPiece.C` implements piece shapes, movement, rotation, and landing.
  `game/BTPieceManager.C` implements the original weighted piece selection,
  including dice and happy pieces. Reuse these rather than recreate modern
  Tetris rules in JavaScript.
- `game/BTBoardManager.C` owns collision and line removal, but stores `BTBox`
  objects and invokes drawing methods. Its rules also inspect weapon state.
  `game/BTBox.H` includes Xlib and pixmap types. This is the first useful seam:
  supply headless boxes with the same cell IDs and values, and read board state
  for rendering. A headless box must preserve die and happy/frown semantics.
- `game/BTRingNode.C` carries synchronous internal game events. This is reusable
  in-process messaging, distinct from the network protocol.
- `game/BTGame.C` combines game orchestration with widgets, input, and Xt timers.
  `game/BTTimeOut.H` includes X11 Intrinsics. Extract scheduling behind explicit
  time advancement instead of trying to run the Xt event loop inside a page.
- `game/BTComputer.C` contains reusable placement evaluation, but the AI is not
  platform-independent as a whole. It includes `BTGame`, uses communication,
  scoring and weapon managers, and explicitly pumps `XtAppPending` and
  `XtAppProcessEvent`. Its header includes `BTTimeOut.H`. The portable planner
  reuses the evaluation in `game/BTCBoard.C` without compiling the whole Xt-based
  computer controller.
- `game/BTScoreManager.C`, `BTWeaponManager.C`, `BTBazaar.C`, and `BTCOrders`
  connect scoring, shopping, and attacks. Full combat requires these systems
  and their event ordering, not merely displaying a second board.
- `game/BattleTris.C` initializes X11/Motif and loads X resources;
  `widget/` implements the display and widget wrappers. The browser UI should
  replace these presentation responsibilities.
- `sockets/` provides TCP and Xt callbacks; `daemons/` uses native processes;
  `db/` uses POSIX file I/O and locking. Keep server services outside the page.
- `game/BTSoundManager.C` selects effects; `audio/DevAudio.C` contains the
  configured Sun-audio backend. The sound assets are still missing according
  to `README.md`. Browser audio is a later adapter, not a reason to delete
  native sound code.
- `share/` holds resources and the weapons database; `art/` holds PPM/XPM
  artwork. Convert or package selected assets when the frontend needs them.

## Approaches Considered

1. **Emscripten C++ core with Canvas and DOM: recommended first.** Preserve the
   game rules and expose reset, input, time advancement, and state reads through
   a narrow interface. Canvas draws cells; DOM handles menus, controls, and
   shopping. A page can run locally without a game server for solo play.
   This requires replacing platform coupling, but that work also makes native
   tests and future frontends possible. Emscripten documents limited Xlib
   support, not a complete Motif desktop environment; its supported browser
   runtime expects cooperative callbacks that return control to the browser.
   See [Emscripten runtime environment](https://emscripten.org/docs/porting/emscripten-runtime-environment.html).

2. **SDL frontend using the same portable core: good broader desktop option.**
   SDL supports an Emscripten target and can share rendering, input, and audio
   code across browser and desktop. It still requires replacing Motif widgets
   and Xt scheduling, including implementing the bazaar UI. Choose this if a
   new native Windows/macOS/Linux UI becomes a priority. Canvas/DOM is a smaller
   initial browser integration for this simple grid and dialog-heavy game.
   See [SDL Emscripten guidance](https://wiki.libsdl.org/SDL2/README-emscripten).

3. **Remote X11 desktop through noVNC: quickest preservation experiment.** Run
   the existing native application on a server with an X display and VNC
   server; noVNC displays that desktop in the browser using WebSockets, with
   a WebSocket proxy where required. This retains Motif and native gameplay,
   but runs the game on hosted machines, with session management and network
   latency. It is useful for reference behavior and demonstrations rather
   than satisfying local WASM execution.
   See [noVNC](https://github.com/novnc/noVNC).

4. **An entire guest OS in v86: possible preservation route.** v86 emulates an
   x86 machine in the browser and supports Linux guests. A compatible x86 Linux
   guest could run an X11/Motif build; this does not directly execute the
   original SPARC binary. Guest images, X setup, startup costs, and emulator
   overhead make this a larger delivery artifact. Compatibility with this
   application would need an actual guest experiment.
   See [v86](https://github.com/copy/v86).

5. **Port X11, Xt, and Motif themselves to WASM: defer.** This would shift the
   project toward implementing or integrating a window system, toolkit,
   resource handling, fonts, and event dispatch. It could preserve more UI
   code, but no complete compatible stack was established by this research.
   For this codebase, the board/piece seam is a much smaller initial scope.

These tradeoffs are engineering judgments based on the inspected dependencies;
noVNC, SDL, and v86 have not been benchmarked against BattleTris here.

## Implementation Plan and Completion Gates

### Phase 1: Prove original rules can power a browser game

- Add an opt-in `BT_PORTABLE` build using headless boxes and narrow compatibility
  headers. Compile `BTPiece.C`, `BTPieceManager.C`, `BTBoardManager.C`,
  `BTBoard.C`, and `BTRingNode.C` directly from the native source tree.
  This initial solo implementation is present; the next section describes its
  extension to offline Ernie matches.
- Add a C API for a solo session, keyboard/button actions, gravity, and board
  state. Keep presentation and browser lifecycle handling in JavaScript.
- Render the original board dimensions and special cells. Support move, rotate,
  soft/hard drop, pause, restart, and game over.
- Keep the browser build separate from native configure and Motif libraries.
- Gate: native headless rule tests pass; emitted WASM loads and responds to
  actions; a browser can play through landing, pause/resume, and restart.
  Record exactly which checks were actually run.

### Phase 2: Establish parity and a durable platform boundary

The current extension adds per-board deterministic random streams, a bounded
Ernie planner using `BTCBoard.C`, and shared match scheduling. These establish
pieces of the boundary; broad gameplay parity remains a completion gate.

- Add repeatable piece sequences and action traces; compare native and WASM
  state, line totals, die funds, happy-piece timing, top-out, and rotation at
  walls/occupied cells. Cover four-line clears and multiple restarts.
- Compare fixtures against the native Motif game. Compiling the same rules is
  useful, but new session orchestration still needs behavioral comparison.
- Extract shared model interfaces from temporary compatibility shims. Make
  timing, random input, rendering, and transport explicit dependencies.
- Preserve historical piece probabilities and rotation behavior; document any
  intentionally changed controls or scoring.
- Gate: replay fixtures agree across builds and supported browsers; native
  build still compiles. Add sanitizer checks for the extracted core.

### Phase 3: Restore full offline BattleTris

- Extend the current score handling and Ernie evaluator integration to full
  `BTScoreManager` and `BTComputer` behavior. Bounded AI planning, shopping, and
  attack scheduling are present; the native combination strategy is not.
- Validate the interactions and native behavior of all 34 implemented weapons.
  The generated catalogue, bazaar, arsenal, refunds, line durations, and recon
  reports are present; implementation coverage does not establish full parity.
- Recreate the native event chain so score changes, shared bazaar entry,
  purchases, attacks, deaths, and restarts occur in the original order.
- Compare upward gravity and its planner against native play. Upbyside-down,
  Hatter and Slick timers, the 150 ms landing window, and Slide Denied are present.
- Compare recon snapshots, noise, and expiry with native play. Ernie matches
  retain the native free-Condor default and C toggle; reports expose settled
  cells and estimated funds rather than a live falling piece or arsenal.
- Add assets and audio adapters when assets become available.
- Gate: a complete player-versus-computer match, including bazaar and weapon
  use, finishes correctly; fixtures cover each weapon and key interactions.

### Phase 4: Browser multiplayer

The post-offline implementation sequence is now in
[MULTIPLAYER.md](MULTIPLAYER.md). No multiplayer implementation is included.

Browsers cannot directly use the original arbitrary TCP socket connections.
Emscripten offers WebSocket APIs and proxy approaches; its full POSIX-socket
proxy is a separate, more constrained integration, not transparent access to
ordinary TCP servers. See [Emscripten networking](https://emscripten.org/docs/porting/networking.html).

- Map both the server/lobby and peer-game connections in `BTNetManager` and
  `BTCommManager`, including framing and connection state. Do not assume one
  WebSocket-to-TCP tunnel covers the complete topology.
- For an interoperability spike, add a WebSocket gateway to native services
  and test actual message framing, disconnects, and native/browser matches.
- For a durable Internet service, prefer an explicit versioned protocol over
  secure WebSockets and a server that owns match state, validates inputs,
  coordinates opponents, and handles reconnects. Keep ranks and database
  writes on the server.
- Specify integer widths and byte order; do not transmit host pointers or rely
  on compiler struct layout. Add tests for malformed and truncated packets.
- Gate: two browsers complete a match across separate networks, including
  bazaar, weapons, disconnect/reconnect behavior, and outcome recording.
  Native interoperability is a separate gate if retained.

### Phase 5: Universal delivery

- Verify current Chrome, Firefox, Safari, keyboard-only play, touch controls,
  responsive layout, focus changes, and tab suspension.
- Package static assets with clear load failures and cache/version handling.
  Add local preferences or saved sessions only with an explicit persistence
  design; browser in-memory state is not durable storage.
- Add the SDL frontend if maintaining a new portable desktop client is desired.
- Gate: repeatable builds, a documented supported-browser matrix, and an
  explicit release scope that distinguishes solo from full BattleTris.

## Browser Prototype Build

Install and activate the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
so `em++` is on `PATH`. Native tests need a C++ compiler; WASM smoke tests need
Node.js; the local server needs Python 3.

```sh
make -C web test
make -C web
make -C web test-wasm
make -C web serve
```

The generated files live in ignored `web/build/`. Use the local HTTP URL printed
by the server rather than opening the HTML through `file://`. The prototype
requires no X11, Motif, database daemon, or multiplayer service. See `web/` for
its implementation and current controls. Passing these prototype checks does
not establish full native-game parity or completion of phases 2 through 5.

For browser checks, keep the server running at `http://127.0.0.1:8000/` and run
the following from the repository root in another terminal. Playwright can be
installed outside the repository:

```sh
npm install --prefix /tmp/battletris-ui-checks playwright@1.62.1
node /tmp/battletris-ui-checks/node_modules/playwright/cli.js install chromium firefox webkit
NODE_PATH=/tmp/battletris-ui-checks/node_modules node web/tests/browsers.cjs
```

Pass browser names after `browsers.cjs` to select a subset, such as `firefox
webkit`. The runner exits nonzero on failure, prints results and browser
versions, and saves touch screenshots in ignored `.playwright-mcp/`.

### Current behavior and compatibility limits

Choose solo practice or an offline Ernie match; mode and pace selections take
effect on Restart. Ernie has three piece intervals: Lethargic (2000 ms), Focused
(750 ms), and Caffeinated (300 ms). The portable planner explores reachable
positions with a breadth-first search capped at 4096 states, scores candidate
landings with the original `BTCBoard` evaluator, and spreads the chosen movement
path across the selected interval. This retains the evaluator but changes search
and movement scheduling; it is not a claim of identical native Ernie behavior.

The match advances in fixed 10 ms simulation increments. Player gravity remains
512 ms with no line-based speed progression. Pause, focus loss, and tab hiding
stop both boards; a win, loss, or draw freezes the match. Catch-up is capped after
slow frames. Failed gravity or manual step-down starts a 150 ms landing grace
period. Lateral movement and rotation remain available without resetting the
timer; further manual step-down is ignored during grace. At expiration, the game
retries one downward move: escape resumes gravity, while a blocked piece locks.
Pause and bazaar entry stop the timer; resume starts a fresh 150 ms grace
interval. Restart and Swap's fresh spawn clear the landing state.
Hard drop remains immediate and bypasses grace. Single-step soft drop adapts the
native sustained fast-drop control, and keyboard repeat follows the browser/OS
repeat rate.

Both boards use landing grace. Ernie waits for it and replans if the piece
escapes; this is an adaptation from native AI, which skipped the player landing
timer. Slide Denied removes the grace period, so failed descent locks immediately.
It retains its native price and ten affected-player-line duration.

Hatter attempts a rotation every 20 ms, including during landing grace. Slick
moves one column every 20 ms; a blocked move reverses its direction for the next
tick. It starts left and retains direction across pieces. Slick stops during
landing grace and after a human manual down command until the next piece, an
adaptation of native fast-drop suppression. New pieces restart both timer
fractions. Repeated applications extend duration without multiplying speed.
Pause and the bazaar stop these timers too. Resume restarts the full configured
gravity, landing, Hatter, and Slick intervals, matching native
`BTGame::unpauseTimeOut`. The current piece, landing state, Slick direction, and
manual-drop suppression are retained. Repeatedly setting an already running
game to unpaused does not reset its clocks. Browser Ernie's movement-command
and attack timers also restart; its distributed movement remains an adaptation
of native Ernie's single placement delay.

Both effects also disrupt browser Ernie, unlike the native AI's bypasses. Forced
movement invalidates its path; it replans at the next command deadline without
resetting its movement clock. Commands are spaced at least 40 ms apart while
disrupted to bound planning work. Natural gravity runs during these effects so
repeated changes of path cannot prevent descent. Native AI tactic parity is not
claimed for these adaptations.

Upbyside-down flips the settled board vertically, spawns pieces at row 24, and
reverses gravity and horizontal controls. Rotation keeps its original direction,
including Hatter's rotations; Slick follows the reversed horizontal controls.
Repeated applications extend the effect without flipping the board again.
Expiration flips it back before the next spawn. The original drop-score formula
is retained, including its lower starting award for a bottom-spawned piece.

Browser Ernie also plays upward, unlike the native AI's gravity exemption. Its
planner searches legal upward moves and evaluates a vertically mirrored board
and candidate through the original evaluator. Fallout permits pieces to leave
through the top. Swap cancels Upbyside-down on both boards before exchanging
them, returning their gravity and spawn positions to normal.

Player drop input awards `28 - piece_origin_y` once per piece. Ernie receives 14
points per successfully placed piece. Line clearing adds lines and funds, with
no additional score bonus. Funds use the original cleared-cell values multiplied
by the number of cleared lines, including dice and immediate happy-face value.
These formulas come from the native game; the adapted drop controls and timing
still require comparison against native play.

Each portable board owns a xorshift32 stream, seeded independently. Zero seeds
map to one; the update uses shifts 13, 17, and 5, the integer result is the updated
state shifted right by one, and the unit value is that integer divided by 2^31.
The original rejection sampling and piece weights remain, but generated sequences
differ from historical libc random sequences. Native Motif builds keep their
existing RNG behavior. Separate streams prevent AI activity or another session's
reset from advancing the player's sequence.

### Current combat scope

The build generates the catalogue from `share/btweapons.db` and
`share/btweaponsp.db`, retaining native names, descriptions, prices, and durations.
All 34 catalogue effects are offered in the browser bazaar: Feared Weird,
Four-by-Four, Rise Up, Flip Out, Speedy Gonzales, Missing Pieces, Piece It Together,
Blind Cleric, Carter Years, Reagan Era, Have a Nice Day, So Long, No Dice, Bug
Report, Meadow, Mirror Mirror, Twilight Zone, Broken Record, The Force, Gimp,
Swap Meet, Lawyers' Delite, Mondale, Keating Five, Lazy Susan, Fallout,
Bottleneck, Slide Denied, Hatter, Slick Willy, Upbyside-down, William Ames,
Ace of Spies, and The Condor.
Gimp cells render the original default `btgimp2.ppm` artwork, converted losslessly
to PNG and drawn without smoothing.

Offline matches enter the bazaar every 20 combined cleared lines. Both boards
freeze until the player selects Done; Ernie completes shopping immediately.
Inventory has ten slots with duplicate weapons stacked, capped at 32767 copies
per slot. Prices do not increase with repeated purchases. Carter doubles the
recipient's prices while active. Undo refunds only copies purchased during the
current visit, at the paid price; previous inventory cannot be sold. Solo mode
has no bazaar or attacks.

Weapons launch from inventory buttons or keys 1-9 and 0. Incoming effects wait
until the recipient locks a piece, then apply before the next spawn. Repeated
timed effects add duration, measured in affected-player cleared lines. Mirror
marks the opponent so their future attacks rebound onto themselves. Swap Meet,
Lazy Susan, Mondale, Keating Five, Have a Nice Day, Mirror, and all three spies
are consumed but nullified instead of reflected. Pending attacks, effect durations, inventory,
and the latest combat event are visible in the frontend.

Ernie buys up to three affordable weapons per bazaar using a deterministic
priority list and attempts one launch every two seconds. This is an adapted
strategy, not the native combination/ordering system. Speedy affects Ernie's
movement pace in the browser even though the native computer ignored it.
Stacked speed changes all reset when the accumulated duration expires, avoiding
the native residual speed multiplier. Portable piece probabilities are
recomputed when overlapping Feared Weird, So Long, and Four-by-Four effects
expire, preserving effects that are still active.

The peer effects use explicit local-match rules in place of the native
asynchronous communication handshake:

- Swap Meet atomically exchanges settled grids at the recipient's lock boundary,
  preserving full cell metadata. The peer's active piece is replaced with a
  fresh piece, while the recipient spawns after the queued attack batch. Each
  player's counters, RNG stream, and inventory stay with that player; the fresh
  piece consumes that player's own random stream.
- Lazy Susan consumes its launching copy, then exchanges the remaining arsenals
  at the recipient's lock boundary.
- Keating Five transfers the recipient's current signed funds to the peer and
  zeros the recipient. Debt transfers too. This replaces the native delayed
  credit based on a previously reported balance.
- Mondale gives the affected player `gross * 7 / 10` of positive earnings using
  integer truncation; the peer receives the remainder. This conserves funds,
  unlike the native reconstruction from reported net gains. Transfers are
  untaxed. The original 50-line duration remains.
- Lawyers' Delite lasts five lines cleared by the affected player. Each line
  cleared by their peer while it is active queues one Rise Up. Already queued
  rises remain after expiration. Applying them at a piece boundary replaces
  native midpiece, piece-aware rise handling.

Fallout opens the middle six columns below the board, retaining two-column
ledges at either side. Pieces can pass through that opening and be discarded
without earning funds, subject to the original bounded collision rules. The
portable planner explores those offboard paths. Activation removes removable
central cells even while Force is active, while preserving Bottleneck's fixed
structures.

Bottleneck adds fixed outer-three-column structures on rows 10 through 17,
leaving a four-column passage. Expiration removes its structures reliably.
Swap cancels Bottleneck on both boards before exchanging settled grids.

### Reconnaissance and native Ernie visibility

Native [`BTGame::exposeEvent`](usr/src/game/BTGame.C) calls `condor()` when a
computer match starts. The browser follows this behavior: offline Ernie matches
start with a free 65535-line Condor report allowance. C turns recon off, clearing
all current spy effects and cached knowledge; turning it on restores Condor with
65535 lines. As in native Ernie, the first opponent placement activates the
request after its report opportunity, and the second supplies the first report.
Turning C off during this wait cancels the browser's pending request. Solo mode
has no recon.
This preserves the native visibility default without showing the opponent's
falling piece or arsenal.

Paid Ames, Ace, and Condor update the viewer's allowance and quality at launch,
as native `BTRecon` does. Recipient reporting activates at the next lock boundary.
Duration adds to the existing allowance, including free Condor time, and counts
the observed player's cleared lines. A later paid spy can therefore lower the
quality of a free Condor view. Mirror consumes and nullifies all three spies.
If reporting was inactive, newly activated paid spies first report on the
following placement. Existing reports stay cached when another spy is launched.
Lines cleared during the activation placement already consume allowance; expiry
before activation cannot revive an expired report.

Reports are cached snapshots of settled cells and current funds after placement
and line clearing, before queued effects and the next piece spawn. Moving a falling
piece or rendering the page does not refresh the report or consume randomness.
This matches native Ernie's report-before-weapon-flush order: a queued board or
funds mutation appears in the following report rather than the current one.
Ames reports each visible occupied cell with 50 percent probability, Ace with
85 percent, and Condor with 100 percent. Hidden and invisible cells stay hidden.
Ames adds or subtracts a uniformly selected amount from zero through the absolute
fund balance; estimates clamp to the signed integer range. Ace adds or subtracts
up to 99 only on a four-line-clear report. Condor reports exact snapshot funds.
Each viewer has a separate deterministic recon RNG, leaving gameplay streams
unchanged. Caching funds together with the placement report adapts the native
score-update timing and avoids rerolling estimates during display refreshes.

All catalogue weapons now have browser implementations. This does not establish
full native gameplay parity. There is no next-piece preview or saved state.

Shared safety fixes include matching `BTWeapon` string-array allocation with
`delete[]`, bounded empty-cell selection for Piece It Together/Bug Report
so a full target region cannot loop indefinitely, and proper disposal of
boundary rows during line clears with or without Force. These also affect
native source code; they do not require adopting the portable frontend.
Upward Rise Up now also disposes cells pushed beyond the bottom edge instead
of leaking them.

### Validation status

The earlier solo prototype was built with Emscripten 4.0.15. Native rule tests
passed under AddressSanitizer and UndefinedBehaviorSanitizer, and the same tests
passed as WASM with heap/stack checks. Those fixtures covered all 18 pieces,
rotations, collision, die funds, happy-piece timing, pause, top-out, and restart;
a separate Node smoke test exercised exported state and lifecycle calls. The
user also successfully tried that browser prototype.

The expanded native AddressSanitizer/UndefinedBehaviorSanitizer and WASM
heap/stack-check suites pass, including piece rules, reachable AI paths, score,
independent random streams, pause/end states, effects, inventory, and bazaar
behavior. The production exported-module smoke test passes natural bazaar entry
at 20 combined lines, Ernie shopping, frozen boards while shopping, resumption,
a deferred Force attack, and reset.

Twenty-four seeded match/combat traces agree between native and WASM at 720 cumulative
checkpoints, comparing board occupancy, random state, effects, and inventory.
This establishes cross-build consistency for those traces, not full native
Motif gameplay parity. Boundary-row regressions also pass in both builds,
including Force on/off and both gravity directions.

The five added peer effects pass in both test builds: Swap metadata and
active-piece handling, Lazy Susan inventory exchange, signed Keating transfers,
conserving Mondale taxation, Lawyers line trigger/expiry ordering, and Mirror
nullification. Swap coverage includes queued-effect ordering, generation
invalidation, blocked fresh spawns, and preservation of existing losses.

Fallout/Bottleneck tests pass in both builds, including open-floor disposal,
offboard planner paths for all 18 piece types, overlapping Force/geometry
effects, repeated activation, structure cleanup on expiry, and Swap
cancellation followed by another Bottleneck in the same queued attack batch.

Landing-grace and Slide Denied tests pass in both builds: the 150 ms deadline,
lateral escape, rotations without timer resets, ignored repeated down input,
instant hard drop, pause/bazaar freezing, queued attack delivery after lock,
fresh-spawn cleanup, and Ernie's placement with and without the grace period.
Replay comparisons also include gravity and landing-clock state.

Hatter/Slick tests pass in both builds: 19/20 ms deadlines, blocked rotations,
Slick collision reversal, duration stacking and expiry, landing behavior,
manual-down suppression, pause, direction persistence, reset, and equivalent
frame partitions. Ernie makes progress under each effect. Replay fixtures also
exercise both effects together and compare motion revisions and timer state.

Upbyside-down passes both suites: upward paths for all 18 pieces, mirrored
four-line clearing, negative piece origins, Fallout departure through the top,
metadata-preserving flips, repeated activation, expiry/reset, and interactions
with Force, Rise Up, and Bottleneck. Match tests cover reversed controls,
Ernie's progress, and Swap cancellation followed by a fresh queued inversion.

Recon tests pass in both builds, covering queued
activation, cached settled reports without the falling piece, target-line expiry,
quality/duration stacking, Mirror nullification, both viewers, reset, free Condor
and C toggling, solo mode, noise bounds, and independence from gameplay RNG.
The exported-module smoke test also verifies free Condor startup, cached reads,
toggle clearing, waiting for the next report, and disabling recon in solo mode.
Replay comparisons include both viewers' reports and independent random states.

The frontend passes Playwright checks in Chrome 152 and, through the CLI runner
with Playwright 1.62.1, Chromium 151.0.7922.34, Firefox 153.0, and WebKit 26.5
on macOS. The reusable
`web/tests/browser.js` function can be loaded with Playwright MCP's
`browser_run_code_unsafe` filename option while the local server is running.
It instruments only the test page to fix the Restart seed and control frame
time; the original compiled WASM still performs every game action.

Coverage includes Canvas updates, keyboard movement, form-key isolation,
pause/blur, restart, Condor display/toggling, bazaar focus and board freezing,
buy/refund/launch, and solo button controls. A keyboard placement fixture earns
$30 by clearing four lines, then buys and refunds a weapon through the UI;
funds and board state are not injected. A separate uninstrumented browser run
also confirmed real-time play, Condor updates, pause, and no console errors.

Layout checks passed at 1280x900, 768x1024, 390x844, 320x568, 844x390, and
568x320: the board and
movement/pause buttons fit together without horizontal overflow. Screenshots
were inspected at desktop and narrow mobile widths. This led to viewport-sized
boards, controls directly below the player board, compact inventory, and
collapsible settings/instructions. The CLI runner also passes emulated touch
checks in all three engines at 390x844: Play, movement/rotation/drop, pause,
Condor toggling, visible controls, and no uncaught page errors. These checks use
an uninstrumented page with real-time simulation. Firefox supports touch events
but not Playwright's mobile viewport emulation, so its context uses the same
viewport and touch capability without `isMobile`. Firefox and WebKit touch
screenshots were inspected, including landscape controls beside the board.
The expanded suite also completes a loss/restart and verifies retry recovery
after missing script, WASM, or Gimp artwork responses. Both development assets
and content-hashed packaged assets under a URL subpath pass these checks.
Physical iOS/Android devices and branded Safari
remain untested; the WebKit result does not establish full Safari compatibility.

A full native Motif build now succeeds in Debian 12 on ARM64 with GCC 12.2 and
Motif 2.3.8, including the client, both daemons, and referee. The client runs on
Xvfb with local services and starts an Ernie match. Manual screenshots confirm
the initial empty opponent view, later free-Condor settled-board reports, zero
player score under natural gravity, Ernie score in multiples of 14, and pause.
These observations are not a deterministic trace comparison. The macOS native
build still needs XQuartz/Motif validation; `BT_PORTABLE` remains opt-in.

The native build exposed a reversed configure branch: an explicit
`--with-motif-libs` rejected a successful Motif link. Both `configure.in` and
the generated `configure` now put that error in the failure branch. The isolated
build supplies Automake's `install-sh`, absent from the checkout's generated
`usr/bin/` directory.

### Repeatable native smoke check

With Podman running, build from the repository root:

```sh
podman build -t localhost/battletris-native-check -f tests/native/Containerfile .
podman run --name battletris-native-smoke localhost/battletris-native-check
podman cp battletris-native-smoke:/tmp/native-results ./native-results
podman rm battletris-native-smoke
```

No ports are published. The container uses a non-root player, local native
services, and a 1280x1024 virtual X display. The smoke check starts Comatose
Ernie, verifies a paused screenshot remains unchanged, and verifies the display
changes after resume. It has a 45-second timeout. Inspect the captured screens
as well: this is a fixed-layout UI smoke check, not a substitute for comparing
weapon interactions or scores under identical input traces.

Timer boundary tests now cover full 512 ms gravity, 150 ms landing, and 20 ms
Hatter/Slick delays after resume, plus bazaar exit and the adapted AI command
clock. The browser fixture also checks a full gravity interval after keyboard
pause/resume. Native timer behavior is established by source inspection here;
the native smoke test checks freeze/resume visually without measuring intervals.

Terminal cleanup now removes falling pieces, clears pending attacks and recon,
and retains settled boards, scores, funds, and the result. Restart restores a
fresh match. This follows native cleanup while retaining the browser's static
result screen. Native Ernie detects failed spawn at its next scheduled
whole-piece turn; the browser detects it immediately because its AI moves a
visible active piece. Immediate top-out is retained as an intentional scheduler
adaptation, rather than adding an invisible waiting turn. The combined 20-line
bazaar threshold and FIFO weapon flush order have source and regression coverage;
exhaustive identical-input native GUI comparisons remain outside this release's
parity claim.
The native Comatose smoke run also showed score 14 with an empty opponent view
after the first placement, then score 28 with a settled-board report after the
second. Its initial Condor request is queued until the first weapon flush;
the browser now follows that delay at startup and when C re-enables the view.
Native and WASM unit tests, exported-module checks, and the browser UI fixture
check the empty first report and visible second report explicitly.

## Native Targets

The native application targets macOS with XQuartz/OpenMotif and Linux with
X11/OpenMotif. Preserve that path while building the independent browser client.
The repository README describes the modern native version; the original
bring-up checklist below is retained for historical context.

## Build Prerequisites

### macOS

1. **XQuartz** - normally installed at `/opt/X11`
2. **OpenMotif** - install with `brew install openmotif`
   - Headers land in `/opt/homebrew/include`
   - Libraries land in `/opt/homebrew/lib`
3. **Xcode Command Line Tools** (`clang++`)

### Linux (Ubuntu)

System X11 + Motif packages, plus build tools:

```
sudo apt-get install build-essential autoconf \
                     libmotif-dev libxt-dev libxext-dev libx11-dev
```

Headers/libraries land in the standard `/usr/include` and `/usr/lib/x86_64-linux-gnu`
locations, which is where the current `Makeinclude` already points. Compiler is
`g++` (or `clang++` - either works).

## Historical Native Bring-up Checklist

The following checklist records the earlier Solaris-to-macOS/Linux work. It is
not a list of outstanding browser tasks. Several fixes are already present:
modern C++ headers, configured Sun-audio support, and a `msg_control` socket
path. Inspect current code before applying any historical suggestion, especially
the suggestion to replace sound implementations with no-ops.

### Step 1 - Install Motif + X11

macOS:
```
brew install openmotif
```

Linux (Ubuntu):
```
sudo apt-get install libmotif-dev libxt-dev libxext-dev libx11-dev
```

### Step 2 - Run configure
From `usr/src/`, run `./configure`. It was written for Solaris so it will likely
need hints for Motif and X11 paths.

macOS:
```
./configure --with-motif=/opt/homebrew --x-includes=/opt/X11/include --x-libraries=/opt/X11/lib
```

Linux (Ubuntu) - system paths usually work without flags:
```
./configure
```

Inspect the generated `Makeinclude` and `BTConfig.H` to make sure paths are correct.

### Step 2b - Build system (Sun-make-isms)
The Makefiles were written for Sun `make`, which supplied implicit rules that
GNU `make` doesn't. Two are patched in tree:
- `Makeinclude` / `Makeinclude.in` define a `$(DSTINCDIR)/%.H: %.H` pattern rule
  so subdirectory `Makefile`s can list installed headers as dependencies
  without an explicit recipe.
- The top-level `Makefile` has a `dirs:` target that `mkdir -p`s `../include`,
  `../lib`, `../bin` before any subdir tries to install into them.

Keep these in place when touching the build system on either platform.

### Step 3 - Attempt a build, collect errors
```
make 2>&1 | tee build.log
grep -c error: build.log
```

### Step 4 - Fix pre-standard C++ errors
This is the bulk of the work. Expected issues with modern clang++:

- `#include <iostream.h>` -> `#include <iostream>` + `using namespace std;`
- `#include <fstream.h>` -> `#include <fstream>`
- `#include <strstream.h>` -> `#include <sstream>` (also: `ostrstream` -> `ostringstream`)
- `#include <string.h>` may need `<cstring>`
- Missing `std::` prefix on `cout`, `cerr`, `endl`, `string`, etc.
- `NULL` vs `nullptr` - leave as `NULL`, it's fine
- Old-style cast syntax - may generate warnings, not errors
- `for` loop variable scoping - old compilers allowed `for(int i=...)` to leak scope
- Template syntax issues - old compilers were lenient; clang++ is not

### Step 5 - Stub out Sun audio
`usr/src/audio/` talks directly to `/dev/audio` (Solaris only). The simplest fix
is to make `BTSoundManager` a no-op:
- In `BTSoundManager.C`, gut the implementation so all methods return immediately
- Audio is entirely optional - the game is fully playable without it

### Step 6 - Fix platform-specific issues

**Common to both targets** (Solaris-isms that need to go):
- `#include <sys/filio.h>` (Solaris) -> `#include <sys/ioctl.h>`
- `#include <sys/select.h>` may be needed
- `bzero()` / `bcopy()` - may need `<strings.h>`
- Solaris socket options that don't exist on either macOS or Linux

**macOS specifics**:
- `SIGPOLL` - not available on macOS; replace with `SIGIO`
- BSD-flavored `<sys/socket.h>` - generally close to Solaris, fewer surprises

**Linux (glibc) specifics**:
- `sockets/StreamSocket.C` redeclares `typedef int socklen_t;` - glibc already
  defines it via `<bits/socket.h>`. Gate the local typedef on a platform check
  or remove it.
- `struct msghdr` on glibc has no `msg_accrights` / `msg_accrightslen` fields
  (those are Solaris/4.3BSD). File-descriptor passing in `StreamSocket::sendfd`
  / `recvfd` must be rewritten to use ancillary data (`msg_control` +
  `CMSG_FIRSTHDR` / `SCM_RIGHTS`). macOS supports the same `msg_control` API,
  so prefer that path on both platforms rather than `#ifdef`-ing two
  implementations.
- `SIGPOLL` is available on Linux but deprecated; `SIGIO` works on both - same
  workaround as macOS.

When adding platform conditionals, prefer probing for features in
`configure.in` over hardcoding `#ifdef __linux__` / `#ifdef __APPLE__` where
possible.
