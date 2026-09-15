# Multiplayer implementation plan

Written after the offline release preparation and local validation completed.
This is a plan only. No multiplayer service, transport, account system, or
ranking storage has been added.

## Recommended first release

Start with private, unranked, two-human browser matches using one authoritative
server over secure WebSockets. Keep the current solo/Ernie game independent and
available without that service. Add public matchmaking, persistent identities,
rankings, spectators, and native interoperability as separate later milestones.

Use the same portable C++ rules on the server. A Node.js transport with one
Emscripten module instance per match is the initial implementation candidate:
the existing module already runs in Node and passes native/WASM replay checks.
Measure memory and throughput before committing to that runtime. A native C++
worker behind the transport remains an alternative if the measurements warrant
it. Never share the current module's static `BrowserMatch` between rooms.

This recommendation follows the code's existing separation of rules and UI.
WebSocket has broad browser support, but its API does not provide automatic
backpressure; bounded queues are part of the design. See
[MDN WebSocket API](https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API).

## What the native network actually does

- `usr/src/game/BTNetManager.C` opens a listening socket, registers its address
  with the daemon, fetches roster/player data, and opens separate peer
  connections for challenges. Accept/start messages then hand the peer socket
  to the game communication manager.
- `usr/src/game/BTCommManager.C` exchanges scores, weapons, boards, arsenals,
  pause, bazaar, and game-over messages. Received weapons are queued and flushed
  at a piece boundary. Local Ernie uses sibling communication managers instead
  of the remote socket path.
- `usr/src/sockets/PacketBuffer.C` frames messages with type and byte count.
  Individual payloads have their own encodings. `sendBoard` allocates using
  `sizeof(unsigned long)` while applying network-long helpers, so cross-width
  behavior must be audited rather than assumed safe.
- `usr/src/game/BTProtocol.H` mixes local game events, challenge messages,
  daemon coordination, and database requests. Its enum is a source reference,
  not the proposed public protocol.

A single WebSocket-to-TCP tunnel does not solve both daemon discovery and the
client's incoming peer socket. Emscripten documents socket proxies, including a
full POSIX proxy with additional constraints and overhead. Keep that as a
separate preservation experiment if native/browser interoperability becomes a
requirement. See [Emscripten networking](https://emscripten.org/docs/porting/networking.html).

## 1. Extract a two-human match controller

Refactor `web/Match.C/H` so controller type and player side are explicit. Replace
the hard-coded computer opponent with two human sessions, each using player
gravity and scoring. Keep the Ernie controller as an optional existing adapter.
Expose side-specific input, purchase, refund, ready-for-bazaar-exit, surrender,
and authorized pause operations. Networking must not call mutable fields directly.

Retain the 10 ms simulation step. Define the within-tick order for simultaneous
inputs, placement, lines/funds, report generation, FIFO attacks, bazaar entry,
and deaths. Choose and test simultaneous top-out semantics; current side-order
execution is not sufficient evidence of fairness. Both bazaar participants must
be ready before the server resumes the match.

Add server-side state serialization with all cell metadata, current piece,
random states, timers, inventory, pending attacks, reconnaissance and controller
state. The current display snapshots and cumulative hashes cannot restore a
match. Keep serialization separate from the per-player network view.
Include piece-manager selection/cycle state and weapon application counts as
well as durations; the current piece and RNG alone do not reproduce future play.
The existing browser `bt_tick` caps each call at 100 ms. Add an explicit server
catch-up budget and overload policy rather than silently losing elapsed time
by feeding long wall-clock gaps to that browser adapter.

Gate: two scripted human controllers finish matches with every weapon enabled;
replays and save/restore agree in native and WASM builds, and offline Ernie tests
remain unchanged. Include simultaneous Swap/Susan/Keating, spy expiry during a
clear, first bazaar, both bazaar exits, and same-tick deaths.

## 2. Define protocol and private state views

Start with bounded JSON messages for inspectability. Specify a protocol version,
rules/catalog fingerprint, room ID, connection epoch, monotonically increasing
input sequence, server tick, command type, and validated payload. The server
assigns the acting player from the authenticated connection, not a client field.
Use bounded integers with documented ranges; do not transmit native structs,
pointers, or host-width values. Binary encoding can follow measured need.

Client commands: create/join, ready, input, buy, refund, launch, bazaar-ready,
pause-request/accept, resume, surrender, and reconnect. Server responses:
welcome, room state, match start, acknowledged input sequence, player snapshot,
bazaar state, pause state, result, and structured rejection.

Only the server advances time, generates pieces, resolves purchases, applies
weapons, and declares results. Reject wrong-phase commands, invalid slots,
unaffordable purchases, duplicate/stale sequences, nonfinite values, and oversized
messages. Initial limits to tune: 4 KiB inbound commands, 16 KiB snapshots, and
60 gameplay commands/second with a bounded burst. These are proposed game limits,
not library defaults or benchmarked capacity.

Send a viewer only their own board, inventory and funds, public opponent score/
lines, and their permitted reconnaissance report. Do not send the opponent's
raw board, arsenal, RNG seed/state, or full checkpoint to a browser. Existing
`bt_op_cells`, funds, and side-selectable arsenal exports are local/test APIs;
they must not become remote response fields. The free Condor default applies
to Ernie matches. Human matches start without that Ernie-only allowance.
Own-board snapshots and any later prediction must also respect Bug Report and
Twilight visibility; server checkpoints contain hidden cell metadata that must
not be sent merely to make prediction convenient.

Gate: protocol fixtures cover every command and rejection; reconnect snapshots
preserve the same visibility; inspection of all outbound messages finds no
hidden opponent state. Fuzz malformed/truncated payloads and sequence boundaries.

## 3. Build private rooms and a playable baseline

Add a transport service and a browser online-mode adapter. Use a single server
process initially, in-memory rooms, and unguessable invitation/session tokens.
Room links identify invitations; reconnect credentials identify the authorized
seat. A second tab cannot silently take over an occupied seat. Expire abandoned
rooms and idle sessions. Keep online mode visibly separate from offline restart.

The first client renders server snapshots without prediction. Run simulation
at 100 Hz and initially send snapshots at 20 Hz, plus immediate phase/result
events. Coalesce obsolete snapshots, keep reliable control messages ordered,
and disconnect persistently slow consumers. Bound inbound and outbound queues;
monitor outgoing `bufferedAmount`. See
[MDN bufferedAmount](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/bufferedAmount).

Gate: two independent browsers on separate networks complete a full match with
bazaar, purchases, attacks, recon, result, and rematch. Test desktop and actual
mobile devices at this stage, with 50/150/300 ms round-trip latency and jitter.
Measure input-to-visible-response latency and server tick lag before adding
prediction or increasing update frequency.

## 4. Handle disconnects, lifecycle, and abuse

Proposed casual-match policy: a disconnected seat gets one bounded 30-second
reconnect grace period with server-controlled suspension. Reconnection replaces
the socket epoch, supplies a fresh private snapshot, and rejects old inputs.
One missing player after grace forfeits; both missing players abandon the room
without a ranked result. Repeated disconnects cannot grant unlimited pauses.
An explicit pause requires server approval and a limited shared pause budget.
These policies need playtesting before a public service.

Tab hiding stops local input and requests suspension; it cannot unilaterally
freeze an online match as the offline client does. Server time and the agreed
pause policy decide progression. Handle page exit, network changes, sleep/wake,
duplicate sockets, reconnect during bazaar, and reconnect after a final result.

Require TLS, check the exact Origin allowlist at upgrade, authenticate the seat,
authorize every room command, and bound connections, messages, and room creation.
Log outcomes and protocol failures without logging credentials. WebSocket
authentication/origin/message validation guidance is covered by the
[OWASP WebSocket Security Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/WebSocket_Security_Cheat_Sheet.html).

Gate: disconnect/reconnect scenarios do not duplicate purchases or attacks,
change outcomes, expose hidden state, or leave permanent rooms. Saturation and
malformed-message tests keep memory bounded and protect healthy matches.

## 5. Decide persistence and responsiveness from measurements

If latency tests require prediction, predict only the local board using server
supplied piece information. Add explicit checkpoint/reconciliation support,
discard acknowledged inputs, and replay remaining validated local inputs.
Do not expose opponent seeds to enable client-side simulation of both boards.
Keep the simpler authoritative-only path until prediction proves correct across
queued attacks, gravity changes, bazaar, and terminal states.

Before rankings, add durable match IDs, authenticated identities, transactional
result recording, and idempotency. The server owns outcomes; a browser cannot
submit its claimed win. Record rules version and input/event trace for diagnosis.
Define server-crash behavior: the first private beta can explicitly abandon
in-memory matches; a ranked service needs recoverable checkpoints or a clear
no-result policy. Do not promise reconnect across a server restart beforehand.

Gate: replayed completed matches reproduce results; repeated result writes are
idempotent; measured concurrency meets an agreed capacity target without tick
drift. Only then choose multi-process room placement and shared storage.

## 6. Optional native interoperability

Inventory and test the entire native topology and payload format, including
32/64-bit board and database fields. Build a constrained gateway with explicit
message translation and native challenge lifecycle handling. Do not expose a
general-purpose arbitrary TCP proxy. Native clients trust peer-provided scores
and game events, so native interoperability needs its own trust/ranking policy.

Gate: browser/native matches pass the same bazaar, weapon, recon, disconnect,
and result fixtures. This milestone must not block browser-to-browser delivery.

## First implementation slice

When multiplayer implementation is authorized, begin with milestone 1: a
transport-independent two-human controller and deterministic fixtures. Then
write protocol fixtures before adding room transport. Hosting provider, public
matchmaking, accounts/rankings, and native interoperability remain explicit
later choices. No deployment or service changes are part of this plan.
