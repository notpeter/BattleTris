# Multiplayer test fixtures

`integration.cjs` opens actual WebSocket connections to the service and uses the
compiled WASM game. It tests two separate rooms and both human seats, private
snapshots, protocol rejection, sequences, pause and reconnect limits, lobby
recovery, results, and event-log replay.

`bazaar-inputs.json` contains 115 placements (483 input commands) for a fresh
two-human match with seed 42. Each entry is `[side, commands]`. It was generated
by a temporary native helper using `BTPlanner` and the original `BTCBoard`
evaluator. For shorter input sequences the helper considered actions in the
order left, right, rotate, down; it replaced each plan's final run of down
commands with hard drop. This does not alter production planning or game rules.
Without elapsed time the sequence clears 13 and 8 lines, earning $444 and $168.

The integration test sends these commands through authenticated player sockets
at a bounded rate while the server's real clock continues running. It does not
inject board state, money, inventory, or weapons. The resulting bazaar verifies
earned purchases and refunds, both players' readiness, a paid Condor's queued
delivery and later report, and deterministic replay after the attack.

`web/tests/online.cjs` separately drives two independent browser contexts through
invitations, readiness, gameplay, mutual pause, responsive layouts, reload
reconnection, and surrender.

## Short load and latency diagnostic

Run `node server/tests/load.cjs` after building WASM and installing the server
dependencies. It starts a fresh child server for each scenario, so its CPU and
RSS measurements exclude the load clients. The scenarios use 1, 4, and 8 active
rooms on loopback, followed by one room with synthetic 50, 150, and 300 ms RTT.
Each sample lasts three seconds; `SAMPLE_MS` can select 1000 through 30000 ms.

Both players issue alternating left/right controls on a 100 ms timer. Only
real protocol commands are used. Bidirectional link callbacks retain order and
apply a deterministic jitter pattern of +/- 10% of the one-way delay. Even the
zero-delay scenario passes through scheduled callbacks, so acknowledgment
latency includes their scheduling overhead. This emulates transport delay and
does not measure actual distant networks, mobile radios, bandwidth limits, TCP
congestion, or TLS.

The JSON lines report acknowledgement p50/p95, received application payload
bandwidth (excluding WebSocket/TCP/TLS headers), snapshot frequency, observed
authoritative tick progression, and child server CPU/RSS/event-loop delay.
CPU percentage is relative to one core; event-loop delay uses Node's 10 ms
histogram resolution. Room initialization is excluded. All submitted controls
must receive acknowledgements and all rooms must remain active.

These short runs are diagnostics, not capacity or latency guarantees. They
exercise movement and gravity, not the CPU cost of prolonged matches or every
weapon. Run longer representative matches on the intended host before choosing
a public room limit.

`load-baseline.json` and `load-results.json` record representative local runs
before and after unchanged weapon catalogs stopped being resent. The baseline
sent 29 controls per player; the final harness sends its first control
immediately and recorded 30. Compare application payload bandwidth with that
small workload difference in mind. A three-second tick-rate estimate also has
snapshot-boundary error; it is not a measurement of long-term clock drift.
