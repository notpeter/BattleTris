const assert = require('node:assert/strict');
const WebSocket = require('ws');
const { createService } = require('../index.cjs');

const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
class Client {
  constructor(url, origin = 'http://localhost') {
    this.messages = [];
    this.seq = 0;
    this.socket = new WebSocket(url, { origin });
    this.open = new Promise((resolve, reject) => {
      this.socket.once('open', resolve);
      this.socket.once('error', reject);
    });
    this.socket.on('message', data => this.messages.push(JSON.parse(data)));
  }
  async wait(predicate, after = 0) {
    const deadline = Date.now() + 5000;
    while (Date.now() < deadline) {
      const found = this.messages.slice(after).find(predicate);
      if (found) return found;
      await delay(10);
    }
    throw new Error('Message timeout: ' + JSON.stringify(this.messages.slice(-3)));
  }
  async send(type, fields = {}, sequenced = true) {
    await this.open;
    this.socket.send(JSON.stringify({ v: 1, type,
      ...(sequenced ? { seq: ++this.seq } : {}), ...fields }));
  }
  state(after = 0, predicate = () => true) {
    return this.wait(message => message.type === 'state' && predicate(message), after);
  }
  async command(type, fields = {}) {
    const after = this.messages.length;
    await this.send(type, fields);
    return this.state(after, state => state.ack === this.seq);
  }
  async close() {
    if (this.socket.readyState === WebSocket.CLOSED) return;
    await new Promise(resolve => {
      this.socket.once('close', resolve);
      this.socket.close();
    });
  }
}

(async () => {
  const service = await createService({ origins: ['http://localhost'], graceMs: 300,
    pauseMs: 200, roomIdleMs: 10000, seedFactory: () => 42 });
  const clients = [];
  const address = await service.listen(0, '127.0.0.1');
  const url = `ws://127.0.0.1:${address.port}`;
  const client = () => { const value = new Client(url); clients.push(value); return value; };
  async function room() {
    const host = client();
    await host.send('create', {}, false);
    const first = await host.wait(message => message.type === 'joined');
    const guest = client();
    await guest.send('join', { room: first.room, invite: first.invite }, false);
    const second = await guest.wait(message => message.type === 'joined');
    assert.equal(first.side, 0);
    assert.equal(second.side, 1);
    assert.notEqual(first.token, second.token);
    assert.notEqual(first.token, first.invite);
    assert.equal(second.invite, undefined, 'Guest must not receive invitation secret');
    const waiting = await host.command('ready');
    assert.equal(waiting.phase, 'waiting', 'One ready player must not start the game');
    await guest.command('ready');
    await host.state(0, state => state.own && state.phase === 'playing');
    return { host, guest, first, second };
  }
  try {
    const base = `http://127.0.0.1:${address.port}`;
    assert.equal((await fetch(base + '/assets/%00.png')).status, 400);
    assert.equal((await fetch(base + '/healthz')).status, 200);
    const a = await room();
    const b = await room();
    const invalid = client();
    await invalid.open;
    for (const payload of ['{', JSON.stringify({ v: 99, type: 'create' }),
      JSON.stringify({ v: 1, type: 'join', room: a.first.room, invite: a.first.token }),
      JSON.stringify({ v: 1, type: 'reconnect', room: a.first.room, token: a.first.invite })]) {
      const after = invalid.messages.length;
      invalid.socket.send(payload);
      await invalid.wait(message => message.type === 'error', after);
    }
    await invalid.close();
    let state = await a.host.command('input', { command: 4, side: 1 });
    assert(state.own.score > 0, 'Authenticated host controls its own board');
    assert.equal(state.opponent.score, 0, 'Client side field cannot impersonate peer');
    const score = state.own.score;
    const marker = a.host.messages.length;
    a.host.socket.send(JSON.stringify({ v: 1, type: 'input', command: 4, seq: a.host.seq }));
    await delay(80);
    state = await a.host.state(marker);
    assert.equal(state.own.score, score, 'Duplicate command cannot lock another piece');
    state = await a.guest.command('input', { command: 4 });
    assert(state.own.score > 0, 'Second human can control its board');
    const untouched = await b.host.state(0, value => value.own && value.phase === 'playing');
    assert.equal(untouched.own.score, 0, 'Rooms must have isolated WASM instances');
    assert.equal(untouched.opponent.score, 0);
    await b.host.command('pause');
    const paused = await b.guest.command('pause');
    assert.equal(paused.phase, 'paused', 'Both players must approve suspension');
    const activeCheckpoint = await service.exportCheckpoint(b.first.room);
    const activeReplay = await service.restoreCheckpoint(activeCheckpoint);
    assert.equal(activeReplay.tick, paused.tick);
    assert.deepEqual(activeReplay.views[1].own, paused.own, 'Active checkpoint preserves private board state');
    assert.deepEqual(activeReplay.views[1].recon, paused.recon);
    const timeoutStart = b.host.messages.length;
    await b.host.state(timeoutStart, value => value.phase === 'playing');
    const exhaustedStart = b.host.messages.length;
    await b.host.send('pause');
    await b.host.wait(message => message.type === 'error', exhaustedStart);
    for (const peer of [a.host, a.guest, b.host, b.guest]) {
      for (const snapshot of peer.messages.filter(value => value.type === 'state' && value.own)) {
        assert.equal(snapshot.own.cells.length, 280);
        assert.equal(snapshot.own.inventory.length, 10);
        assert.equal(snapshot.catalog.length, 34);
        assert.deepEqual(Object.keys(snapshot.opponent).sort(), ['lines', 'score']);
        assert(!snapshot.recon.known, 'Human matches have no free Condor');
        assert(!snapshot.recon.cells || snapshot.recon.cells.every(cell => cell === 0),
          'Unknown reconnaissance cannot contain hidden board cells');
        assert.equal(snapshot.seed, undefined);
        assert.equal(snapshot.rng, undefined);
        assert.equal(snapshot.token, undefined);
      }
    }
    const purchaseStart = a.host.messages.length;
    await a.host.send('buy', { token: 0 });
    await a.host.wait(message => message.type === 'error', purchaseStart);
    const invalidInput = a.host.messages.length;
    await a.host.send('input', { command: 999 });
    await a.host.wait(message => message.type === 'error', invalidInput);
    for (const type of ['constructor', 'toString', '__proto__']) {
      const after = a.host.messages.length;
      await a.host.send(type);
      await a.host.wait(message => message.type === 'error', after);
    }
    const takeover = client();
    await takeover.send('reconnect', { room: a.first.room, token: a.first.token }, false);
    await takeover.wait(message => message.type === 'error');
    await takeover.close();
    const beforeDisconnect = await a.host.command('input', { command: 0 });
    await a.host.close();
    const disconnected = await a.guest.state(0, value => value.phase === 'reconnecting');
    const frozenStart = a.guest.messages.length;
    await delay(60);
    const frozen = await a.guest.state(frozenStart, value => value.phase === 'reconnecting');
    assert.equal(frozen.tick, disconnected.tick, 'Reconnect grace suspends authoritative time');
    assert.deepEqual(frozen.own.cells, disconnected.own.cells);
    const returning = client();
    await returning.send('reconnect', { room: a.first.room, token: a.first.token }, false);
    const joined = await returning.wait(message => message.type === 'joined');
    returning.seq = joined.ack;
    assert.equal(joined.ack, a.host.seq, 'Reconnect restores sequence acknowledgement');
    const restored = await returning.state(0, value => Boolean(value.own));
    assert.equal(restored.own.score, beforeDisconnect.own.score);
    assert.deepEqual(restored.own.inventory, beforeDisconnect.own.inventory);
    await returning.close();
    await a.guest.state(a.guest.messages.length, value => value.phase === 'ended');
    await b.host.command('surrender');
    const ended = await b.guest.state(0, value => value.phase === 'ended');
    const checkpoint = await service.exportCheckpoint(b.first.room);
    const replay = await service.restoreCheckpoint(checkpoint);
    assert.equal(replay.tick, ended.tick, 'Replay restores exact simulation tick');
    for (const key of ['own', 'recon', 'status', 'result']) {
      assert.deepEqual(replay.views[1][key], ended[key], 'Replay restores ' + key);
    }
    const lobbyHost = client();
    await lobbyHost.send('create', {}, false);
    const lobbyJoined = await lobbyHost.wait(message => message.type === 'joined');
    await lobbyHost.command('ready');
    await lobbyHost.close();
    const lobbyGuest = client();
    await lobbyGuest.send('join', { room: lobbyJoined.room, invite: lobbyJoined.invite }, false);
    await lobbyGuest.wait(message => message.type === 'joined');
    await lobbyGuest.command('ready');
    const lobbyReturning = client();
    await lobbyReturning.send('reconnect', { room: lobbyJoined.room, token: lobbyJoined.token }, false);
    const lobbyRestored = await lobbyReturning.wait(message => message.type === 'joined');
    lobbyReturning.seq = lobbyRestored.ack;
    const lobbyState = await lobbyReturning.state();
    assert.equal(lobbyState.ready[0], false, 'Disconnected lobby player can ready again');
    await lobbyReturning.command('ready');
    await lobbyGuest.state(0, value => value.phase === 'playing');
    await lobbyReturning.command('surrender');
    // Seed 42 placements generated with BTPlanner's original board evaluator.
    // Only real inputs are replayed: no server memory or board injection.
    const shopping = await room();
    const shoppers = [shopping.host, shopping.guest];
    const nextInput = [0, 0];
    for (const [side, commands] of require('./bazaar-inputs.json')) {
      await delay(Math.max(0, nextInput[side] - Date.now()));
      const player = shoppers[side], after = player.messages.length;
      for (const command of commands) await player.send('input', { command });
      const moved = await player.state(after, value => value.ack === player.seq);
      assert.notEqual(moved.phase, 'ended', 'Placement fixture must reach the bazaar');
      nextInput[side] = Date.now() + commands.length * 17;
      if (moved.phase === 'bazaar') break;
    }
    let shop = await shopping.host.state(0, value => value.phase === 'bazaar');
    assert(shop.own.lines + shop.opponent.lines >= 20, 'Real line clears open the bazaar');
    const condor = shop.catalog.find(item => /condor/i.test(item.name));
    const funds = shop.own.funds;
    shop = await shopping.host.command('buy', { token: condor.token });
    assert(shop.own.funds < funds);
    shop = await shopping.host.command('refund', { slot: 0 });
    assert.equal(shop.own.funds, funds, 'Refund restores earned money');
    await shopping.host.command('buy', { token: condor.token });
    const oneReady = await shopping.host.command('bazaar-ready');
    assert.equal(oneReady.phase, 'bazaar');
    await shopping.guest.command('bazaar-ready');
    const launched = await shopping.host.command('launch', { slot: 0 });
    assert.equal(launched.own.inventory[0].quantity, 0);
    await shopping.guest.command('input', { command: 4 });
    let spy = await shopping.host.command('input', { command: 0 });
    assert(!spy.recon.known, 'Paid spy waits for the next victim placement');
    await shopping.guest.command('input', { command: 4 });
    spy = await shopping.host.state(shopping.host.messages.length, value => value.recon.known);
    assert.equal(spy.recon.cells.length, 280);
    const shoppingEnd = await shopping.host.command('surrender');
    const shoppingReplay = await service.restoreCheckpoint(await service.exportCheckpoint(shopping.first.room));
    assert.deepEqual(shoppingReplay.views[0].own, shoppingEnd.own,
      'Replay preserves earned purchases and launched attacks');
    console.log('Online bazaar: real line clears, earned funds, buy/refund, two-player ready, paid Condor delivery/report and attack replay passed');
    const denied = new WebSocket(url, { origin: 'https://untrusted.invalid' });
    await new Promise((resolve, reject) => {
      denied.once('open', () => { denied.close(); reject(new Error('Invalid Origin accepted')); });
      denied.once('error', resolve);
    });
    const oversized = client();
    await oversized.open;
    const closed = new Promise(resolve => oversized.socket.once('close', resolve));
    oversized.socket.send('x'.repeat(4097));
    await Promise.race([closed, delay(2000).then(() => { throw new Error('Oversized frame retained'); })]);
    const flooding = client();
    await flooding.open;
    const rateClosed = new Promise(resolve => flooding.socket.once('close', resolve));
    for (let n = 0; n < 160; n++) flooding.socket.send('{}');
    assert.equal(await Promise.race([rateClosed, delay(2000).then(() => {
      throw new Error('Command flood was not disconnected');
    })]), 1008, 'Command rate limit closes abusive clients');
    console.log('Online integration: isolated human rooms, private snapshots, checkpoint replay, sequence and phase validation, bounded pause/reconnect, lobby recovery, forfeit, surrender, Origin, payload and rate limits passed');
  } finally {
    await Promise.all(clients.map(value => value.close()));
    await service.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
