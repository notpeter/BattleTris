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
