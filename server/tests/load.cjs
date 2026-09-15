'use strict';
// Diagnostic only: loopback sockets and synthetic ordered delay are not a WAN.
const assert = require('node:assert/strict');
const { fork } = require('node:child_process');
const { performance, monitorEventLoopDelay } = require('node:perf_hooks');
const WebSocket = require('ws');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

async function child() {
  const { createService } = require('../index.cjs');
  const service = await createService({ origins: ['http://localhost'], seedFactory: () => 42 });
  const histogram = monitorEventLoopDelay({ resolution: 10 });
  histogram.enable();
  let cpu, started, peakRss = 0;
  const sample = setInterval(() => { peakRss = Math.max(peakRss, process.memoryUsage().rss); }, 50);
  const address = await service.listen(0, '127.0.0.1');
  process.send({ type: 'listening', port: address.port });
  process.on('message', async message => {
    if (message.type === 'begin') {
      cpu = process.cpuUsage(); started = performance.now(); histogram.reset();
      peakRss = process.memoryUsage().rss;
      process.send({ type: 'begun' });
    } else if (message.type === 'measure') {
      const elapsedMs = performance.now() - started, used = process.cpuUsage(cpu);
      process.send({ type: 'metrics', elapsedMs, cpuMs: (used.user + used.system) / 1000,
        cpuPercentOneCore: (used.user + used.system) / (elapsedMs * 10),
        rssMiB: process.memoryUsage().rss / 1048576, peakRssMiB: peakRss / 1048576,
        eventLoopDelayP95Ms: histogram.percentile(95) / 1e6,
        eventLoopDelayMaxMs: histogram.max / 1e6 });
    } else if (message.type === 'stop') {
      clearInterval(sample); histogram.disable(); await service.close(); process.disconnect();
    }
  });
}

// Each direction preserves order, as a single WebSocket connection does.
// Jitter repeats deterministically at +/- 10% of one-way delay.
class OrderedLink {
  constructor(rtt) { this.oneWay = rtt / 2; this.lastDue = 0; this.index = 0; this.timers = new Set(); }
  enqueue(callback) {
    const wave = [0, 1, -1, 0.5, -0.5][this.index++ % 5];
    const due = Math.max(performance.now() + this.oneWay * (1 + wave * 0.1), this.lastDue);
    this.lastDue = due;
    const timer = setTimeout(() => { this.timers.delete(timer); callback(); }, Math.max(0, due - performance.now()));
    this.timers.add(timer);
  }
  clear() { for (const timer of this.timers) clearTimeout(timer); }
}
class Client {
  constructor(url, rtt) {
    this.up = new OrderedLink(rtt); this.down = new OrderedLink(rtt);
    this.seq = 0; this.pending = new Map(); this.latencies = []; this.bytes = 0;
    this.states = 0; this.errors = []; this.measuring = false;
    this.socket = new WebSocket(url, { origin: 'http://localhost' });
    this.open = new Promise((resolve, reject) => { this.socket.once('open', resolve); this.socket.once('error', reject); });
    this.socket.on('error', error => this.errors.push(error.message));
    this.socket.on('message', bytes => this.down.enqueue(() => {
      const message = JSON.parse(bytes);
      if (this.measuring) this.bytes += bytes.length;
      if (message.type === 'joined') this.joined = message;
      if (message.type === 'error') this.errors.push(message.code + ': ' + message.message);
      if (message.type !== 'state') return;
      this.state = message;
      if (this.measuring) this.states++;
      for (const [seq, at] of this.pending) if (seq <= message.ack) {
        this.latencies.push(performance.now() - at); this.pending.delete(seq);
      }
    }));
  }
  async send(type, fields = {}, sequenced = false) {
    await this.open;
    const message = { v: 1, type, ...fields };
    if (sequenced) message.seq = ++this.seq;
    if (type === 'input') this.pending.set(message.seq, performance.now());
    this.up.enqueue(() => {
      if (this.socket.readyState === WebSocket.OPEN) this.socket.send(JSON.stringify(message));
    });
  }
  async wait(predicate) {
    const deadline = performance.now() + 10000;
    while (!predicate()) {
      assert(performance.now() < deadline, 'Client deadline expired: ' + JSON.stringify(this.errors));
      await delay(5);
    }
  }
  close() { this.up.clear(); this.down.clear(); this.socket.terminate(); }
}
function waitMessage(child, type) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => done(new Error('Child deadline: ' + type)), 15000);
    const message = value => { if (value.type === type) done(null, value); };
    const exit = code => done(new Error('Server exited early: ' + code));
    function done(error, value) {
      clearTimeout(timeout); child.off('message', message); child.off('exit', exit);
      if (error) reject(error); else resolve(value);
    }
    child.on('message', message); child.once('exit', exit);
  });
}
const rounded = value => Math.round(value * 100) / 100;
function percentile(values, fraction) {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.min(sorted.length - 1, Math.floor(sorted.length * fraction))] || 0;
}
async function scenario(rooms, rttMs, durationMs) {
  const server = fork(__filename, ['--server'], { stdio: ['ignore', 'inherit', 'inherit', 'ipc'] });
  const clients = [];
  let controls;
  try {
    const { port } = await waitMessage(server, 'listening');
    for (let room = 0; room < rooms; room++) {
      const host = new Client(`ws://127.0.0.1:${port}/ws`, rttMs); clients.push(host);
      await host.send('create'); await host.wait(() => host.joined);
      const guest = new Client(`ws://127.0.0.1:${port}/ws`, rttMs); clients.push(guest);
      await guest.send('join', { room: host.joined.room, invite: host.joined.invite });
      await guest.wait(() => guest.joined);
      await host.send('ready', {}, true); await guest.send('ready', {}, true);
      await Promise.all([host.wait(() => host.state?.phase === 'playing'), guest.wait(() => guest.state?.phase === 'playing')]);
    }
    // Exclude room initialization and handshake from the active measurement.
    await delay(rttMs + 100);
    const begun = waitMessage(server, 'begun'); server.send({ type: 'begin' }); await begun;
    const start = performance.now(), startTicks = clients.map(client => client.state.tick);
    for (const client of clients) client.measuring = true;
    let commandsPerPlayer = 0;
    const sendControls = () => {
      const command = commandsPerPlayer++ % 2; // Legal left/right movement, no injected state.
      for (const client of clients) client.send('input', { command }, true);
    };
    sendControls(); controls = setInterval(sendControls, 100);
    await delay(durationMs);
    clearInterval(controls); controls = null;
    const elapsedMs = performance.now() - start;
    const tickRates = clients.map((client, index) => (client.state.tick - startTicks[index]) * 1000 / elapsedMs);
    const bytes = clients.reduce((sum, client) => sum + client.bytes, 0);
    const snapshots = clients.reduce((sum, client) => sum + client.states, 0);
    for (const client of clients) client.measuring = false;
    const measured = waitMessage(server, 'metrics'); server.send({ type: 'measure' });
    const metrics = await measured;
    await Promise.all(clients.map(client => client.wait(() => client.pending.size === 0)));
    for (const client of clients) {
      assert.deepEqual(client.errors, []);
      assert.equal(client.state.phase, 'playing', 'Room stopped during benchmark');
      assert.equal(client.latencies.length, commandsPerPlayer, 'Every control must be acknowledged');
    }
    const latencies = clients.flatMap(client => client.latencies);
    const { type, ...processMetrics } = metrics;
    return { rooms, players: clients.length, syntheticRttMs: rttMs, elapsedMs: rounded(elapsedMs),
      controlsPerPlayer: commandsPerPlayer, ackP50Ms: rounded(percentile(latencies, 0.5)),
      ackP95Ms: rounded(percentile(latencies, 0.95)),
      receivedPayloadKiBPerSec: rounded(bytes / 1024 / (elapsedMs / 1000)),
      receivedPayloadKiBPerPlayerSec: rounded(bytes / clients.length / 1024 / (elapsedMs / 1000)),
      snapshotsPerPlayerSec: rounded(snapshots / clients.length / (elapsedMs / 1000)),
      tickRateMin: rounded(Math.min(...tickRates)), tickRateMax: rounded(Math.max(...tickRates)),
      server: Object.fromEntries(Object.entries(processMetrics).map(([key, value]) => [key, rounded(value)])) };
  } finally {
    clearInterval(controls); clients.forEach(client => client.close());
    if (server.connected) {
      const stopped = new Promise(resolve => server.once('exit', resolve));
      server.send({ type: 'stop' });
      const kill = setTimeout(() => server.kill('SIGKILL'), 3000);
      await stopped; clearTimeout(kill);
    } else server.kill();
  }
}
if (process.argv.includes('--server')) child().catch(error => { console.error(error); process.exit(1); });
else (async () => {
  const durationMs = Number(process.env.SAMPLE_MS || 3000);
  assert(Number.isSafeInteger(durationMs) && durationMs >= 1000 && durationMs <= 30000,
    'SAMPLE_MS must be an integer from 1000 through 30000');
  console.log('Diagnostic loopback benchmark; synthetic ordered latency, not measured WAN latency or a capacity guarantee.');
  console.log(JSON.stringify({ node: process.version, platform: process.platform, arch: process.arch,
    cpu: require('node:os').cpus()[0]?.model, sampleMs: durationMs }));
  for (const [rooms, rtt] of [[1, 0], [4, 0], [8, 0], [1, 50], [1, 150], [1, 300]]) {
    console.log(JSON.stringify(await scenario(rooms, rtt, durationMs)));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
