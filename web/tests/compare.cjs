const assert = require("node:assert/strict");
const { execFileSync } = require("node:child_process");

const native = execFileSync("./build/core-test", ["--replay"], { encoding: "utf8" });
const wasm = execFileSync(process.execPath, ["build/core-test.js", "--replay"], { encoding: "utf8" });
assert.equal(wasm, native, "Native and WASM replay snapshots differ");
assert.equal(native.trim().split("\n").length, 1440);
console.log("Native/WASM parity passed: 48 seeded Ernie/human combat traces, 1440 cumulative snapshot checkpoints.");
