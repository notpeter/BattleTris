'use strict';
// One isolated WASM instance per room. Browsers send intent, never game state.
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { performance } = require('node:perf_hooks');
const { WebSocketServer, WebSocket } = require('ws');
const build = path.resolve(__dirname, '../web/build');
const createGame = require(path.join(build, 'battletris.js'));
// Allow only the offline startup guard as an inline script.
const bootstrapHashes = [...fs.readFileSync(path.join(build, 'index.html'), 'utf8')
  .matchAll(/<script>([\s\S]*?)<\/script>/g)]
  .map(match => `'sha256-${crypto.createHash('sha256').update(match[1]).digest('base64')}'`).join(' ');
const rules = crypto.createHash('sha256').update(fs.readFileSync(path.join(build, 'battletris.wasm'))).digest('hex');
const secret = () => crypto.randomBytes(24).toString('base64url');
const equal = (a, b) => typeof a === 'string' && typeof b === 'string' && Buffer.byteLength(a) === Buffer.byteLength(b) &&
  crypto.timingSafeEqual(Buffer.from(a), Buffer.from(b));
const integer = (value, min, max) => Number.isSafeInteger(value) && value >= min && value <= max;
const calls = { input: ['_bt_side_input', 'command', 0, 4], buy: ['_bt_side_buy', 'token', 0, 33],
  refund: ['_bt_side_refund', 'slot', 0, 9], launch: ['_bt_side_launch', 'slot', 0, 9],
  'bazaar-ready': ['_bt_side_ready'], surrender: ['_bt_side_surrender'] };
const array = (game, pointer) => Array.from(game.HEAP32.subarray(pointer >> 2, (pointer >> 2) + 280));
function seat() { return { socket: null, token: secret(), ack: 0, ready: false, deadline: null, graceLeft: null, disconnectedAt: null }; }
function model(game, seed) {
  return { id: crypto.randomBytes(9).toString('base64url'), invite: secret(), game, seed,
    catalog: Array.from({ length: game._bt_weapon_count() }, (_, token) => ({ token,
      name: game.UTF8ToString(game._bt_weapon_name(token)),
      description: game.UTF8ToString(game._bt_weapon_description(token)),
      duration: game._bt_weapon_duration(token) })),
    seats: [seat(), seat()], started: false, ended: false, forcedResult: null, tick: 0, events: [],
    paused: false, disconnectPaused: false,
    created: Date.now(), touched: Date.now(), message: '' };
}
function invoke(room, op, args) {
  if (room.events.length >= 100000) {
    room.ended = true; room.forcedResult = 'draw'; room.message = 'Room event limit reached.';
    return 0;
  }
  const result = room.game[op](...args);
  const last = room.events.at(-1);
  if (op === '_bt_tick' && last?.op === op) last.count++;
  else room.events.push({ op, args, count: 1 });
  return result;
}
function phase(room) {
  if (room.ended || (room.started && [2, 3, 4].includes(room.game._bt_status()))) return 'ended';
  if (!room.started) return 'waiting';
  if (room.seats.some(s => !s.socket)) return 'reconnecting';
  if (room.paused) return 'paused';
  return room.game._bt_status() === 5 ? 'bazaar' : 'playing';
}
function view(room, side) {
  const g = room.game, status = room.started ? g._bt_status() : null, current = phase(room);
  const result = current !== 'ended' ? null : room.forcedResult ||
    (status === 4 ? 'draw' : (status === (side === 0 ? 3 : 2) ? 'win' : 'loss'));
  const snapshot = { v: 1, type: 'state', room: room.id, side, tick: room.tick,
    ack: room.seats[side].ack, phase: current, status, result,
    ready: room.seats.map(s => s.ready), connected: room.seats.map(s => Boolean(s.socket)),
    own: null, opponent: null, recon: null,
    catalog: [], linesUntilBazaar: 0, message: room.message };
  if (!room.started) return snapshot;
  snapshot.own = { cells: array(g, g._bt_side_cells(side)), score: g._bt_side_score(side),
    lines: g._bt_side_lines(side), funds: g._bt_side_funds(side), pending: g._bt_side_pending(side),
    bazaarReady: Boolean(g._bt_side_ready_state(side)),
    inventory: Array.from({ length: 10 }, (_, slot) => ({ token: g._bt_arsenal_token(side, slot),
      quantity: g._bt_arsenal_quantity(side, slot), refundable: g._bt_side_refundable(side, slot) })),
    effects: [] };
  snapshot.opponent = { score: g._bt_side_score(1 - side), lines: g._bt_side_lines(1 - side) };
  const known = Boolean(g._bt_side_recon_known(side));
  snapshot.recon = { known, token: g._bt_side_recon_token(side), remaining: g._bt_side_recon_remaining(side),
    funds: known ? g._bt_side_recon_funds(side) : null,
    cells: known ? array(g, g._bt_side_recon_cells(side)) : null };
  for (let token = 0; token < g._bt_weapon_count(); token++) {
    snapshot.catalog.push({ ...room.catalog[token], price: g._bt_side_price(side, token) });
    const remaining = g._bt_remaining(side, token);
    if (remaining) snapshot.own.effects.push({ token, remaining });
  }
  snapshot.linesUntilBazaar = g._bt_lines_until_bazaar();
  return snapshot;
}
async function createService(options = {}) {
  const origins = new Set(options.origins || ['http://localhost:8080', 'http://127.0.0.1:8080']);
  const graceMs = options.graceMs ?? 30000;
  const roomIdleMs = options.roomIdleMs ?? 300000, maxRooms = options.maxRooms ?? 32;
  const rooms = new Map(), connections = new Set(), perIP = new Map();
  let creating = 0, closed = false;
  const server = http.createServer((request, response) => {
    let pathname;
    try { pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname); }
    catch { response.writeHead(400).end(); return; }
    if (pathname.includes('\0')) { response.writeHead(400).end(); return; }
    if (!['GET', 'HEAD'].includes(request.method)) { response.writeHead(405).end(); return; }
    if (pathname === '/healthz') { response.writeHead(200, { 'Content-Type': 'text/plain' }).end('ok\n'); return; }
    if (pathname === '/') pathname = '/online.html';
    const file = path.resolve(build, '.' + pathname);
    if (!file.startsWith(build + path.sep)) { response.writeHead(404).end(); return; }
    const types = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
      '.css': 'text/css; charset=utf-8', '.wasm': 'application/wasm', '.png': 'image/png', '.json': 'application/json' };
    if (!types[path.extname(file)]) { response.writeHead(404).end(); return; }
    fs.stat(file, (error, stat) => {
      if (error || !stat.isFile()) { response.writeHead(404).end(); return; }
      response.writeHead(200, { 'Content-Type': types[path.extname(file)], 'Content-Length': stat.size,
        'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff',
        'Referrer-Policy': 'no-referrer', 'Content-Security-Policy': `default-src 'self'; script-src 'self' 'wasm-unsafe-eval' ${bootstrapHashes}; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'` });
      if (request.method === 'HEAD') response.end();
      else fs.createReadStream(file).on('error', () => response.destroy()).pipe(response);
    });
  });
  const wss = new WebSocketServer({ noServer: true, maxPayload: 4096, perMessageDeflate: false });
  function send(socket, message) {
    if (!socket || socket.readyState !== WebSocket.OPEN) return;
    if (socket.bufferedAmount > 262144) { socket.terminate(); return; }
    // Send catalog metadata on connection and when this viewer's prices change.
    if (message.type === 'state' && message.own) {
      const prices = message.catalog.map(item => item.price).join(',');
      if (socket.catalogPrices === prices) {
        message = { ...message };
        delete message.catalog;
      } else socket.catalogPrices = prices;
    }
    socket.send(JSON.stringify(message));
  }
  const error = (socket, code, message) => send(socket, { v: 1, type: 'error', code, message });
  function broadcast(room) {
    room.seats.forEach((s, side) => { if (s.socket) send(s.socket, view(room, side)); });
  }
  function attach(socket, room, side, host = false) {
    const s = room.seats[side];
    if (s.disconnectedAt !== null) s.graceLeft = Math.max(0, s.graceLeft - (Date.now() - s.disconnectedAt));
    s.socket = socket; s.deadline = null; s.disconnectedAt = null; socket.room = room; socket.side = side;
    send(socket, { v: 1, type: 'joined', rules, room: room.id, side, token: s.token, ack: s.ack,
      ...(host ? { invite: room.invite } : {}) });
    if (room.started && room.seats.every(s => s.socket) && room.disconnectPaused) {
      room.disconnectPaused = false;
      if (!room.paused) invoke(room, '_bt_set_paused', [0]);
      room.message = 'Both players reconnected.';
    }
    broadcast(room);
  }
  function unpause(room) {
    room.paused = false;
    if (!room.disconnectPaused) invoke(room, '_bt_set_paused', [0]);
  }
  server.on('upgrade', (request, socket, head) => {
    const ip = request.socket.remoteAddress;
    if (closed || !['/ws', '/'].includes(request.url) || !origins.has(request.headers.origin) ||
        connections.size >= 64 || (perIP.get(ip) || 0) >= 16) {
      socket.end('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n'); return;
    }
    wss.handleUpgrade(request, socket, head, ws => {
      ws.ip = ip; connections.add(ws); perIP.set(ip, (perIP.get(ip) || 0) + 1);
      wss.emit('connection', ws);
    });
  });
  wss.on('connection', socket => {
    socket.alive = true; socket.credit = 120; socket.refilled = performance.now(); socket.opened = Date.now();
    socket.on('pong', () => { socket.alive = true; });
    socket.on('error', () => {}); // malformed/oversized frames are closed by ws
    socket.on('close', () => {
      connections.delete(socket);
      const remaining = (perIP.get(socket.ip) || 1) - 1;
      if (remaining) perIP.set(socket.ip, remaining); else perIP.delete(socket.ip);
      const room = socket.room;
      if (!room || room.seats[socket.side].socket !== socket) return;
      const s = room.seats[socket.side]; s.socket = null; s.disconnectedAt = Date.now();
      if (s.graceLeft === null) s.graceLeft = graceMs;
      s.deadline = Date.now() + s.graceLeft;
      if (!room.started) s.ready = false;
      if (room.started && phase(room) !== 'ended') {
        room.disconnectPaused = true; invoke(room, '_bt_set_paused', [1]);
        room.message = 'Connection lost. Waiting for reconnect.';
      }
      broadcast(room);
    });
    async function handleMessage(bytes, binary) {
      const now = performance.now();
      socket.credit = Math.min(120, socket.credit + (now - socket.refilled) * 0.06); socket.refilled = now;
      if (--socket.credit < 0) { socket.close(1008, 'Rate limit'); return; }
      let msg;
      try { if (binary) throw Error(); msg = JSON.parse(bytes.toString()); }
      catch { error(socket, 'invalid-message', 'Expected a JSON text message.'); return; }
      if (!msg || Array.isArray(msg) || msg.v !== 1 || typeof msg.type !== 'string') {
        error(socket, 'invalid-message', 'Unsupported protocol message.'); return;
      }
      if (!socket.room) {
        if (socket.busy) return;
        if (msg.type === 'create') {
          if (rooms.size + creating >= maxRooms) { error(socket, 'capacity', 'Server room capacity reached.'); return; }
          socket.busy = true; creating++;
          try {
            const game = await createGame();
            if (closed || socket.readyState !== WebSocket.OPEN) return;
            const seed = options.seedFactory ? options.seedFactory() >>> 0 : crypto.randomBytes(4).readUInt32LE();
            const room = model(game, seed); rooms.set(room.id, room); attach(socket, room, 0, true);
          } catch { error(socket, 'unavailable', 'Could not initialize a match.'); }
          finally { socket.busy = false; creating--; }
          return;
        }
        const room = typeof msg.room === 'string' ? rooms.get(msg.room) : null;
        if (!room) { error(socket, 'room-not-found', 'Room not found or expired.'); return; }
        if (msg.type === 'join') {
          if (!equal(msg.invite, room.invite)) { error(socket, 'invalid-invite', 'Invalid invitation.'); return; }
          if (room.started || room.seats[1].socket || room.seats[1].deadline !== null) {
            error(socket, 'room-full', 'This invitation already has two players.'); return;
          }
          attach(socket, room, 1); return;
        }
        if (msg.type === 'reconnect') {
          const side = room.seats.findIndex(s => equal(msg.token, s.token));
          if (side < 0) { error(socket, 'invalid-token', 'Invalid reconnect token.'); return; }
          if (room.seats[side].socket) { error(socket, 'seat-connected', 'Player is already connected.'); return; }
          if (room.seats[side].deadline !== null && Date.now() > room.seats[side].deadline && phase(room) !== 'ended') {
            error(socket, 'room-not-found', 'Reconnect grace period expired.'); return;
          }
          attach(socket, room, side); return;
        }
        error(socket, 'not-joined', 'Create, join or reconnect first.'); return;
      }
      const room = socket.room, side = socket.side, s = room.seats[side];
      if (s.socket !== socket) return;
      if (!integer(msg.seq, 1, Number.MAX_SAFE_INTEGER)) { error(socket, 'invalid-sequence', 'Expected a positive sequence number.'); return; }
      if (msg.seq <= s.ack) { send(socket, view(room, side)); return; }
      s.ack = msg.seq;
      const reject = message => { error(socket, 'invalid-command', message); send(socket, view(room, side)); };
      const current = phase(room);
      if (current === 'ended') { reject('Match has ended. Create a new room to play again.'); return; }
      if (msg.type === 'ready') {
        if (room.started) { reject('The match has already started.'); return; }
        s.ready = true;
        if (room.seats.every(s => s.ready && s.socket)) {
          room.started = true; room.seats.forEach(s => { s.graceLeft = graceMs; }); room.game._bt_online_start(room.seed); room.message = 'Match started.';
        }
      } else if (msg.type === 'pause') {
        if (!['playing', 'bazaar'].includes(current)) { reject('Pause is unavailable.'); return; }
        room.paused = true;
        invoke(room, '_bt_set_paused', [1]); room.message = 'Match paused.';
      } else if (msg.type === 'resume') {
        if (current !== 'paused') { reject('The match is not paused.'); return; }
        unpause(room); room.message = 'Match resumed.';
      } else {
        const call = Object.hasOwn(calls, msg.type) ? calls[msg.type] : null;
        if (!call || (call[1] && !integer(msg[call[1]], call[2], call[3]))) { reject('Invalid command or value.'); return; }
        if (!room.started || current === 'reconnecting' ||
            (msg.type !== 'surrender' && ((['buy', 'refund', 'bazaar-ready'].includes(msg.type) && current !== 'bazaar') ||
             (['input', 'launch'].includes(msg.type) && current !== 'playing')))) {
          reject('Command is unavailable in this match phase.'); return;
        }
        const args = call[1] ? [side, msg[call[1]]] : [side];
        if (!invoke(room, call[0], args)) { reject('The game rejected that action.'); return; }
      }
      room.touched = Date.now(); broadcast(room);
    }
    socket.on('message', (bytes, binary) => {
      handleMessage(bytes, binary).catch(() => socket.close(1011, 'Message processing failed'));
    });
  });
  let last = performance.now(), remainder = 0, lastSnapshot = 0;
  const timer = setInterval(() => {
    const now = performance.now(), elapsed = now - last; last = now;
    // Abort an overloaded match instead of silently discarding authoritative time.
    if (elapsed > 1000) {
      for (const room of rooms.values()) if (room.started && phase(room) !== 'ended') {
        room.ended = true; room.forcedResult = 'draw'; room.message = 'Match stopped: server could not keep time.'; broadcast(room);
      }
      remainder = 0;
    } else remainder += elapsed;
    const steps = Math.floor(remainder / 10); remainder -= steps * 10;
    for (const [id, room] of rooms) {
      for (const s of room.seats) if (s.deadline !== null && Date.now() >= s.deadline && phase(room) !== 'ended') {
        if (room.started && room.seats.some(s => s.socket)) invoke(room, '_bt_side_surrender', [room.seats.indexOf(s)]);
        else { room.ended = true; room.forcedResult = 'draw'; }
        room.message = 'Reconnect grace period expired.';
      }
      if (room.started && !room.ended && !room.paused && !room.disconnectPaused && ![2, 3, 4].includes(room.game._bt_status())) {
        for (let n = 0; n < steps && !room.ended; n++) {
          if (room.events.length >= 100000) { invoke(room, '_bt_tick', [10]); break; }
          invoke(room, '_bt_tick', [10]); room.tick++;
        }
      }
      if (phase(room) !== 'ended' && (room.events.length > 100000 || Date.now() - room.created > 1800000)) {
        room.ended = true; room.forcedResult = 'draw'; room.message = 'Room time limit reached.';
      }
      if ((phase(room) === 'waiting' || phase(room) === 'ended') && Date.now() - room.touched > roomIdleMs) {
        room.seats.forEach(s => s.socket?.close(1000, 'Room expired')); rooms.delete(id); continue;
      }
      if (now - lastSnapshot >= 50) broadcast(room);
    }
    if (now - lastSnapshot >= 50) lastSnapshot = now;
    for (const socket of connections) if (!socket.room && Date.now() - socket.opened > 10000) socket.close(1008, 'Join timeout');
  }, 10);
  const heartbeat = setInterval(() => {
    for (const socket of connections) {
      if (!socket.alive) { socket.terminate(); continue; }
      socket.alive = false; socket.ping();
    }
  }, 15000);
  timer.unref(); heartbeat.unref();
  function exportCheckpoint(id) {
    const room = rooms.get(id);
    if (!room || !room.started) throw Error('No started room');
    return JSON.parse(JSON.stringify({ v: 1, rules, seed: room.seed, tick: room.tick, events: room.events,
      ended: room.ended, forcedResult: room.forcedResult }));
  }
  async function restoreCheckpoint(checkpoint) {
    if (!checkpoint || checkpoint.v !== 1 || checkpoint.rules !== rules || !integer(checkpoint.seed, 0, 0xffffffff) ||
        !Array.isArray(checkpoint.events) || checkpoint.events.length > 100000) throw Error('Incompatible checkpoint');
    const game = await createGame(), room = model(game, checkpoint.seed); room.started = true;
    game._bt_online_start(room.seed); let ticks = 0;
    const allowed = new Set([...Object.values(calls).map(c => c[0]), '_bt_set_paused', '_bt_tick']);
    for (const event of checkpoint.events) {
      if (!allowed.has(event.op) || !Array.isArray(event.args) || !integer(event.count, 1, 180000) ||
          (event.op !== '_bt_tick' && event.count !== 1)) throw Error('Invalid checkpoint event');
      if (event.op === '_bt_tick') {
        if (event.args.length !== 1 || event.args[0] !== 10 || (ticks += event.count) > 180000) throw Error('Invalid checkpoint clock');
      } else if (event.op === '_bt_set_paused') {
        if (event.args.length !== 1 || !integer(event.args[0], 0, 1)) throw Error('Invalid checkpoint pause');
      } else {
        const call = Object.values(calls).find(c => c[0] === event.op);
        if (event.args.length !== (call[1] ? 2 : 1) || !integer(event.args[0], 0, 1) ||
            (call[1] && !integer(event.args[1], call[2], call[3]))) throw Error('Invalid checkpoint command');
      }
      for (let n = 0; n < event.count; n++) game[event.op](...event.args);
    }
    if (ticks !== checkpoint.tick) throw Error('Invalid checkpoint tick');
    room.tick = ticks; room.ended = checkpoint.ended === true;
    room.forcedResult = checkpoint.forcedResult === 'draw' ? 'draw' : null;
    return { tick: ticks, views: [view(room, 0), view(room, 1)] };
  }
  return { exportCheckpoint, restoreCheckpoint,
    listen: (port = 0, host = '127.0.0.1') => new Promise((resolve, reject) => {
      server.once('error', reject); server.listen(port, host, () => { server.removeListener('error', reject); const address = server.address();
        if (!options.origins) { origins.add(`http://127.0.0.1:${address.port}`); origins.add(`http://localhost:${address.port}`); }
        resolve(address); });
    }),
    close: async () => {
      closed = true; clearInterval(timer); clearInterval(heartbeat);
      for (const socket of connections) socket.terminate();
      await new Promise(resolve => wss.close(resolve));
      await new Promise(resolve => server.close(resolve)); rooms.clear();
    }
  };
}
module.exports = { createService };
if (require.main === module) {
  const port = Number(process.env.PORT || 8080);
  if (!integer(port, 1, 65535)) throw Error('PORT must be between 1 and 65535');
  const origins = (process.env.ORIGINS || `http://localhost:${port},http://127.0.0.1:${port}`).split(',');
  for (const origin of origins) { const parsed = new URL(origin); if (!['http:', 'https:'].includes(parsed.protocol) || parsed.origin !== origin) throw Error('ORIGINS must contain exact HTTP(S) origins'); }
  createService({ origins }).then(async service => {
    const address = await service.listen(port, process.env.HOST || '127.0.0.1');
    console.log(`BattleTris listening on http://${address.family === 'IPv6' ? `[${address.address}]` : address.address}:${address.port}`);
    for (const signal of ['SIGINT', 'SIGTERM']) process.once(signal, () => { service.close().then(() => process.exit(0)); });
  }).catch(error => { console.error(error.message); process.exitCode = 1; });
}
