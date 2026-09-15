# Browser multiplayer

Private, unranked two-human matches are implemented. A Node.js server runs the
same portable C++ rules in an isolated WebAssembly instance for each room. The
browser sends controls and draws private server snapshots. Solo and Ernie still
run locally through `index.html` without a multiplayer service.

## Run a private server

Use Node.js 24 or newer, Python 3, a C++ compiler, and Emscripten 4.0.15.
From the repository root, with Emscripten active:

```sh
make -C web all
npm ci --ignore-scripts --prefix server
npm start --prefix server
```

Open `http://127.0.0.1:8080/`, create a room, and share the invitation. The second
player opens that link. Both select Ready to play. For a local test, use two
browser profiles or a normal and private window. The offline page is available
at `http://127.0.0.1:8080/index.html`.

The default listener is loopback. To play across a LAN, substitute the server's
actual LAN address in this example, then open that address on both devices:

```sh
HOST=0.0.0.0 PORT=8080 ORIGINS=http://192.168.1.10:8080 npm start --prefix server
```

For internet use, put the service behind an HTTPS reverse proxy, forward `/ws`
with WebSocket upgrade support, and set `ORIGINS` to the exact HTTPS origin
(for example `https://battletris.example`). Multiple allowed origins use commas,
with no spaces or trailing slashes. Keep Node bound to loopback behind the
proxy. Invitation links use the address opened in the browser; a localhost link
will not work on another computer. TLS terminates at the proxy, not in Node.

The server serves `web/build`, with online play at `/` and `/online.html`.
The static archive includes `online.html` and a hashed online script, but a
static host alone cannot run multiplayer. To use that archive with a separate
static frontend, proxy its same-origin `/ws` to the Node service and allow the
frontend's origin. No service has been deployed as part of this implementation.

## Match rules and lifecycle

- Both players use human gravity, scoring, controls, and all 34 weapons.
  Human matches have no free Condor; buy a spy for opponent board/funds reports.
- A bazaar opens at the shared line threshold. Purchases and refunds are private.
  A player who selects Done cannot change purchases; both must finish to resume.
- Inputs are ordered by arrival at the server. Gravity advances both boards
  in 10 ms steps; simultaneous gravity top-outs produce a draw. A sequential
  input that ends a match takes effect before a later arriving input.
- Pause requires both players to agree. Either can resume. The room has a shared
  60-second explicit pause allowance; expiry resumes automatically.
- A lost connection suspends play. Each player has a cumulative 30-second
  reconnect allowance for the match. Reloading the same tab restores its seat
  from session storage and a fresh authoritative snapshot; unacknowledged
  commands are not replayed. An already connected seat cannot be taken over.
- One absent player forfeits after grace expires. If both are absent, the room
  ends without a winner. Surrender ends a match immediately. Use New room for
  another match; there is no in-place rematch negotiation yet.
- Waiting and ended rooms expire after five idle minutes. Rooms have a maximum
  lifetime of 30 minutes. A running match that exceeds this limit, its event
  limit, or a one-second server scheduling stall ends in a draw with a reason.
  Ended results are retained until cleanup and cannot be changed by later input.
- Rooms live in memory. A server restart loses matches and invalidates links.
  There are no accounts, rankings, spectators, or automatic durable recovery.

The client deliberately waits for server responses instead of predicting moves.
Network round-trip time therefore affects control responsiveness. Hiding a tab
stops local input and requests a mutual pause; it does not silently stop the
other player's match. Sound remains a set of placeholders.

## Protocol and privacy

Protocol version 1 uses JSON text over same-origin `/ws`. First messages are
`create`, `join` with `room` and `invite`, or `reconnect` with `room` and `token`.
A `joined` response assigns `side`, a seat `token`, current `ack`, and the SHA-256
`rules` fingerprint of the compiled WASM. Only the creator receives `invite`.
Invitation and seat secrets are separate random 192-bit values. Seat credentials
stay in tab session storage and never appear in invitation URLs.

Subsequent messages include `v: 1` and a positive safe-integer `seq`:

```json
{"v":1,"type":"input","seq":1,"command":0}
```

Commands are `ready`, `input` (0 left, 1 right, 2 rotate, 3 soft drop, 4 hard drop),
`buy` with `token` (0-33), `refund` or `launch` with `slot` (0-9), `bazaar-ready`,
`surrender`, `pause`, and `resume`. The authenticated socket determines the side.
Client-supplied side fields have no authority. Sequences may have gaps; duplicate
or older sequences receive a snapshot without reapplying the action. A new valid
sequence is acknowledged even when the command is rejected, preventing replay
of rejected commands after a phase change.

Responses are `state` snapshots or `error` with `code` and `message`. States
include `tick`, `ack`, phase, readiness/connections, result, own board/funds/
inventory/effects, public opponent score/lines, catalog updates, and the viewer's cached
reconnaissance report. They exclude opponent raw cells, funds, arsenal, seeds,
RNG state, and checkpoints. Own cells retain Bug Report/Twilight filtering.
Phases are `waiting`, `playing`, `paused`, `bazaar`, `reconnecting`, and `ended`.
Only the server advances time or decides purchases, attacks, and outcomes.
The first active snapshot on each socket includes the complete catalog. Later
snapshots omit `catalog` when that viewer's prices are unchanged; the client
retains its last catalog. Price changes, including Carter, send a full updated
catalog. Reconnect always supplies it again. Checkpoint views remain complete.

The service validates exact Origin values at upgrade, accepts only text JSON,
and limits frames to 4 KiB. Each socket has a 60-message/second token bucket with
a burst of 120; exceeding it disconnects. Defaults allow 32 rooms, 64 sockets,
and 16 sockets per source IP. A 10-second handshake timeout and ping/pong checks
clean up idle connections. Per-socket outgoing buffering is capped at 256 KiB;
slow consumers are disconnected. Snapshots arrive at 20 Hz plus command replies.
These are conservative bounds, not a measured public-service capacity guarantee.
A reverse proxy needs its own connection and request limits. The server does
not trust forwarded client-IP headers; behind a proxy its IP cap is shared.

## Replay and checks

`createService()` in `server/index.cjs` exposes privileged in-process
`exportCheckpoint(room)` and `restoreCheckpoint(checkpoint)` methods. Checkpoints
record the seed, rules fingerprint, ordered engine commands, and every fixed
clock step (consecutive ticks are compressed). Restoring replays that exact
history into a new WASM instance and returns private views for verification.
The network has no checkpoint endpoint.

This is an event replay checkpoint, not raw C++ memory serialization. It retains
all deterministic piece selection, timers, attacks, inventory and spy state by
reconstruction, avoiding compiler-dependent memory layouts. It takes time
proportional to match history and rejects incompatible rules or malformed events.
It does not reattach room sockets or persist credentials. Durable failover would
need stored room metadata and a recovery policy in addition to these checks.

```sh
make -C web test test-wasm check-dist
npm test --prefix server
# Playwright must be installed, with browsers downloaded:
NODE_PATH=/path/to/node_modules node web/tests/online.cjs
# Diagnostic measurements, intentionally not a timing-sensitive CI gate:
npm run benchmark --prefix server
```

Native/WASM tests cover two-human gravity/scoring, both-ready bazaar transitions,
bilateral attacks, reflected delivery, spies, pause, surrender, and same-tick
deaths alongside the existing offline weapon and replay suites. Socket tests
exercise isolated rooms, malformed commands, privacy, sequence replay, reconnect,
forfeit, pause limits, Origin/frame/rate bounds and checkpoint reconstruction.
Browser checks exercise two independent contexts in Chromium, Firefox and
WebKit, invitations, keyboard/touch controls, pause, reload reconnect, results,
portrait/landscape layouts, and offline startup/error recovery under server CSP.
Socket checks also cover catalog omission, full reconnect catalog, and real
Carter delivery updating only its recipient's prices.

## Local measurements

The repeatable benchmark starts the server in a separate process and drives real
sockets. A September 15, 2026 run on an Apple M5 Pro with Node 24.16.0 used
three-second samples and ten movement controls per player per second:

- Sending the catalog only when needed reduced received application payload
  from approximately 285 to 53 KiB/s per player, about 81%.
- Eight rooms used about 849 KiB/s aggregate payload bandwidth, 67 MiB server
  RSS, and 7.5% of one CPU core. Loopback acknowledgment p95 was 5.7 ms.
- Synthetic ordered 50/150/300 ms RTT with jitter produced acknowledgment p95
  of 55/164/331 ms. Every submitted control was acknowledged and all rooms stayed
  active. These measure command acknowledgment, not time until a browser paints.

The fixtures and raw results live in `server/tests`. These short runs exclude
room initialization, TLS and transport headers, constrained bandwidth, actual
mobile networks, and prolonged weapon-heavy gameplay. They do not establish
production capacity. The default 16-socket source-IP cap also means a single
reverse proxy can admit only eight complete rooms. Measure on the intended host
before increasing limits or choosing a public deployment size.

Server work was a small part of response time in these samples. Further server
micro-optimization cannot remove round-trip delay; local prediction remains a
separate correctness project if real-device playtesting requires it.

## Next steps

1. Playtest across real devices and separate networks at 50/150/300 ms round-trip
   latency with jitter. Measure input latency, tick lag, memory and bandwidth
   at increasing room counts before selecting a hosting size or public limits.
2. Add local prediction only if those measurements justify its complexity.
   Reconcile acknowledged input without publishing opponent seeds or hidden cells.
3. Decide persistence and identity requirements before accounts or rankings.
   Add durable IDs, idempotent result recording, recoverable room metadata and
   an explicit server-crash outcome policy. Optimize checkpoints if replay costs
   become significant. Multi-process routing follows measured capacity needs.
4. Consider native interoperability independently. The original Motif network
   uses daemon discovery and peer TCP sockets in `BTNetManager.C`, with gameplay
   messages in `BTCommManager.C` and native framing in `PacketBuffer.C`.
   A WebSocket-to-TCP tunnel alone does not handle that topology or its trust
   model. Audit all payload widths, including `sendBoard`'s unsigned-long sizing,
   before writing a constrained translation gateway.

The design follows the [MDN WebSocket API](https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API)
and its [buffering guidance](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/bufferedAmount),
the [ws server documentation](https://github.com/websockets/ws), and
[OWASP WebSocket guidance](https://cheatsheetseries.owasp.org/cheatsheets/WebSocket_Security_Cheat_Sheet.html).
[Emscripten networking](https://emscripten.org/docs/porting/networking.html)
describes socket proxies for a possible future native compatibility experiment.
