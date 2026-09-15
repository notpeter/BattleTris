const assert = require("node:assert/strict");
const { execFileSync } = require("node:child_process");

const native = execFileSync("./build/core-test", ["--replay"], { encoding: "utf8" });
const wasm = execFileSync(process.execPath, ["build/core-test.js", "--replay"], { encoding: "utf8" });
assert.equal(wasm, native, "Native and WASM replay snapshots differ");
assert.equal(native.trim().split("\n").length, 720);
console.log("Native/WASM parity passed: 24 seeded match/combat traces, 720 cumulative snapshot checkpoints.");
