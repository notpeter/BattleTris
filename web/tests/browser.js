// Playwright browser_run_code_unsafe can load this function with its filename option.
// Instrument only the test page: retain the real WASM instance, seed Restart,
// and drive requestAnimationFrame deterministically. No production debug API.
async (page, url = 'http://127.0.0.1:8000/') => {
  page.setDefaultTimeout(5000);
  const assert = Object.assign((ok, message = 'Assertion failed') => { if (!ok) throw new Error(message); }, {
    equal: (a, b) => { if (a !== b) throw new Error(`Expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`); },
    notEqual: (a, b) => { if (a === b) throw new Error('Expected different values'); },
    deepEqual: (a, b) => { if (JSON.stringify(a) !== JSON.stringify(b)) throw new Error('Snapshots differ'); },
    notDeepEqual: (a, b) => { if (JSON.stringify(a) === JSON.stringify(b)) throw new Error('Expected different snapshots'); },
    match: (text, pattern) => { if (!pattern.test(text)) throw new Error(`No match: ${text}`); }
  });
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.addInitScript(() => {
    window.requestAnimationFrame = callback => { window.__frame = callback; return 1; };
    window.__now = 1000;
    window.__advance = ms => {
      for (let left = ms; left > 0; left -= 100) {
        window.__now += Math.min(left, 100);
        window.__frame(window.__now);
      }
    };
    Object.defineProperty(crypto, 'getRandomValues', { value: array => { array.fill(42); return array; } });
  });
  const intercept = async route => {
    const response = await route.fetch();
    await route.fulfill({ response, body:
      'const factory = createBattleTris; createBattleTris = async (...args) => { const game = await factory(...args); window.__game = game; return game; };\n' + await response.text() });
  };
  await page.route('**/app*.js', intercept);
  await page.goto(url);
  try {
    // The fixture owns requestAnimationFrame, so Playwright must poll by timer.
    await page.waitForFunction(() => window.__game && window.__frame, null, { polling: 10, timeout: 15000 });
  } catch (error) {
    const startup = await page.evaluate(() => ({ url: location.href,
      status: document.querySelector('#status')?.textContent,
      game: !!window.__game, frame: !!window.__frame }));
    throw new Error('Game startup failed: ' + JSON.stringify({ ...startup, errors }) + ': ' + error.message);
  }
  // Native tile colors and geometry; rendering must not invent per-client noise.
  const tiles = await page.evaluate(() => {
    const canvas = document.createElement('canvas'); canvas.width = 230; canvas.height = 644;
    const cells = new Int32Array(280); cells.set([3, 24, 20, -1]);
    BattleTrisUI.drawBoard(canvas, cells, new Image());
    const ctx = canvas.getContext('2d');
    return [[0, 0], [22, 22], [31, 8], [46, 0], [69, 0]].map(([x, y]) =>
      Array.from(ctx.getImageData(x, y, 1, 1).data));
  });
  assert.deepEqual(tiles, [[238, 0, 0, 255], [139, 0, 0, 255], [0, 0, 0, 255],
    [191, 191, 191, 255], [0, 0, 0, 255]]);
  const pipSizes = await page.evaluate(() => [18, 25, 33, 47].map(tile => {
    const canvas = document.createElement('canvas'); canvas.width = tile * 10; canvas.height = tile * 28;
    const cells = new Int32Array(280); cells[0] = 28;
    BattleTrisUI.drawBoard(canvas, cells, new Image());
    const pixels = canvas.getContext('2d').getImageData(0, 0, tile, tile).data;
    const visited = new Set(), boxes = [];
    for (let y = 0; y < tile; ++y) for (let x = 0; x < tile; ++x) {
      const start = y * tile + x;
      if (visited.has(start) || pixels[start * 4] !== 168) continue;
      const queue = [start], points = []; visited.add(start);
      while (queue.length) {
        const pos = queue.pop(), px = pos % tile, py = Math.floor(pos / tile); points.push([px, py]);
        for (const [nx, ny] of [[px-1,py],[px+1,py],[px,py-1],[px,py+1]]) {
          const next = ny * tile + nx;
          if (nx >= 0 && nx < tile && ny >= 0 && ny < tile && !visited.has(next) && pixels[next * 4] === 168) {
            visited.add(next); queue.push(next);
          }
        }
      }
      const width = Math.max(...points.map(p => p[0])) - Math.min(...points.map(p => p[0])) + 1;
      const height = Math.max(...points.map(p => p[1])) - Math.min(...points.map(p => p[1])) + 1;
      if (width < tile / 2 && height < tile / 2) boxes.push([width, height]);
    }
    return boxes;
  }));
  for (const boxes of pipSizes) {
    assert.equal(boxes.length, 5);
    assert(boxes.every(([width, height]) => width === height && width === boxes[0][0]),
      'Die pips must remain identical squares at fractional scale');
  }
  assert(await page.locator('#board').isVisible());
  assert.equal(await page.locator('nav [aria-current="page"]').textContent(), 'Offline');
  assert.equal(await page.locator('nav a[href="online.html"]').count(), 1);
  const read = () => page.evaluate(() => {
    const g = window.__game;
    const p = g._bt_cells() >>> 2;
    return { status: g._bt_status(), score: g._bt_score(), funds: g._bt_funds(),
      opScore: g._bt_op_score(), cells: Array.from(g.HEAP32.subarray(p, p + 280)) };
  });
  const advance = ms => page.evaluate(ms => window.__advance(ms), ms);
  const settings = async () => { await page.locator('.settings').evaluate(node => { node.open = true; }); };
  const restart = async () => { await settings(); await page.locator('#restart').click(); };
  assert.equal((await read()).status, 0);
  assert.equal(await page.locator('#level option').count(), 15);
  assert.equal(await page.locator('#level option').first().textContent(), 'Comatose');
  assert.equal(await page.locator('#level option').last().textContent(), 'Bionic');
  await page.getByRole('button', { name: 'About', exact: true }).click();
  assert(await page.getByRole('dialog').isVisible());
  assert.equal(await page.locator('.about-authors img').evaluateAll(images => images.every(image => image.complete && image.naturalWidth > 0)), true);
  await page.getByRole('button', { name: 'OK', exact: true }).click();
  assert(await page.getByRole('dialog').isHidden());
  assert(await page.locator('#opponent-board').isHidden());
  const desktopFits = async () => {
    const original = page.viewportSize();
    for (const [width, height] of [[1024, 640], [1280, 720], [1280, 900], [1920, 1080]]) {
      await page.setViewportSize({ width, height });
      const layout = await page.evaluate(() => {
        const button = document.getElementById('toggle-recon').getBoundingClientRect();
        return { height: innerHeight, scrollHeight: document.documentElement.scrollHeight,
          condorBottom: button.bottom, condorTop: button.top,
          sidebarOverflow: document.querySelector('aside').scrollHeight - document.querySelector('aside').clientHeight,
          centerWidth: document.querySelector('aside').getBoundingClientRect().width,
          topGap: Math.abs(document.querySelector('#board').getBoundingClientRect().top -
            document.querySelector('.score-box').getBoundingClientRect().top) };
      });
      assert(layout.scrollHeight <= height && layout.sidebarOverflow <= 1,
        'Default desktop scrolls: ' + JSON.stringify(layout));
      assert(layout.centerWidth <= 301 && layout.topGap <= 1, 'Board/center alignment: ' + JSON.stringify(layout));
      assert(layout.condorTop >= 0 && layout.condorBottom <= height,
        'Condor control is outside the viewport: ' + JSON.stringify(layout));
    }
    await page.setViewportSize(original);
  };
  await desktopFits(); // Waiting for the first report.
  await page.locator('#pause').focus();
  await page.keyboard.press('Space');
  assert.equal(await page.evaluate(() => document.activeElement.id), 'board');
  const before = await read();
  const beforeCanvas = await page.locator('#board').evaluate(canvas => canvas.toDataURL());
  await page.keyboard.press('ArrowLeft');
  assert.notDeepEqual((await read()).cells, before.cells);
  assert.notEqual(await page.locator('#board').evaluate(canvas => canvas.toDataURL()), beforeCanvas);
  await page.keyboard.press('p');
  const paused = await read();
  await advance(3000);
  assert.deepEqual(await read(), paused);
  await settings();
  await page.locator('#mode').focus();
  await page.keyboard.press('ArrowDown');
  assert.deepEqual(await read(), paused); // Form keys do not move the piece.
  await page.locator('#mode').selectOption('1');
  await page.locator('#level').selectOption('10');
  await restart();
  const nextOpponentPlacement = async () => {
    const scores = await page.evaluate(() => {
      const before = window.__game._bt_op_score();
      for (let i = 0; i < 100 && window.__game._bt_op_score() === before; ++i)
        window.__advance(10);
      return [before, window.__game._bt_op_score()];
    });
    assert.equal(scores[1], scores[0] + 14);
  };
  await nextOpponentPlacement();
  assert(await page.locator('#opponent-board').isHidden());
  await nextOpponentPlacement();
  assert(await page.locator('#opponent-board').isVisible());
  await page.locator('.settings').evaluate(node => { node.open = false; });
  await desktopFits(); // A report must use the same space as the empty spy board.
  await page.keyboard.press('c');
  assert(await page.locator('#opponent-board').isHidden());
  assert.equal(await page.locator('#op-funds').textContent(), '?');
  assert(await page.locator('#opponent-panel').isHidden());
  assert.equal(await page.locator('#toggle-recon').getAttribute('aria-pressed'), 'false');
  await desktopFits(); // Condor disabled.
  await page.keyboard.press('c');
  assert(await page.locator('#opponent-board').isHidden());
  await nextOpponentPlacement();
  assert(await page.locator('#opponent-board').isHidden());
  await nextOpponentPlacement();
  assert(await page.locator('#opponent-board').isVisible());
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  assert.equal((await read()).status, 1);

  // Actual seeded placements earn $30 from four cleared lines. No funds or
  // board-state injection: every command goes through the keyboard handler.
  await restart();
  await advance(1); // Establish the first animation timestamp after Restart.
  const earn = [1,1,1,4,1,4,0,0,4,0,0,0,4,0,0,0,0,4,0,0,0,0,0,4,0,0,4,
    0,0,0,0,0,4,2,2,2,1,4,2,1,1,1,1,1,4,1,1,4,0,0,0,4,0,0,4,4,1,4,1,1,4,
    2,2,0,0,0,0,0,4];
  const keys = ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Space'];
  for (const command of earn) {
    await page.keyboard.press(keys[command]);
    if (command === 4) await advance(450);
  }
  assert.equal((await read()).funds, 30);
  for (let i = 0; i < 500 && (await read()).status === 0; ++i) await advance(100);
  assert.equal((await read()).status, 5);
  assert.equal(await page.evaluate(() => document.activeElement.id), 'bazaar');
  assert.equal(await page.locator('#shop option').count(), 34);
  assert.equal(await page.locator('#inventory-panel').evaluate(node => node.parentElement.id), 'shop-inventory');
  const frozen = await read();
  await advance(2000);
  assert.deepEqual(await read(), frozen);
  const viewport = page.viewportSize();
  for (const width of [1280, 390]) {
    await page.setViewportSize({ width, height: 900 });
    const heights = [];
    for (let token = 0; token < 34; ++token) {
      await page.getByRole('listbox', { name: 'Weapons for sale' }).selectOption(String(token));
      heights.push(await page.locator('#bazaar').evaluate(node => node.getBoundingClientRect().height));
    }
    assert(await page.locator('.weapon-info').evaluate(node => node.scrollHeight <= node.clientHeight), 'Weapon description requires scrolling');
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Bazaar overflows horizontally');
    await page.screenshot({ path: `.playwright-mcp/bazaar-${width}.png` });
    assert(Math.max(...heights) - Math.min(...heights) <= 1, 'Bazaar changes height with selection');
    await page.getByRole('listbox', { name: 'Weapons for sale' }).selectOption('8');
    const dimensions = () => page.evaluate(() => ['#bazaar', '.weapon-list', '#shop-inventory'].map(selector => {
      const r = document.querySelector(selector).getBoundingClientRect(); return [r.width, r.height];
    }));
    const emptyDimensions = await dimensions();
    await page.getByRole('button', { name: 'Buy Flip out', exact: true }).click();
    assert.deepEqual(await dimensions(), emptyDimensions);
    await page.getByRole('button', { name: 'Undo purchase from arsenal slot 1', exact: true }).click();
    assert.deepEqual(await dimensions(), emptyDimensions);
  }
  await page.setViewportSize(viewport);
  await page.getByRole('listbox', { name: 'Weapons for sale' }).selectOption('8');
  const buy = page.getByRole('button', { name: /^Buy Flip out$/i });
  const shopBounds = () => page.evaluate(() => ['bazaar', 'shop', 'inventory-panel'].map(id => {
    const r = document.getElementById(id).getBoundingClientRect(); return [r.width, r.height];
  }).concat([[document.querySelector('.weapon-list').getBoundingClientRect().height]]));
  const beforePurchase = await shopBounds();
  const cash = (await read()).funds;
  await buy.click();
  assert.equal((await read()).funds, cash - 15);
  assert.deepEqual(await shopBounds(), beforePurchase);
  const refund = page.getByRole('button', { name: 'Undo purchase from arsenal slot 1', exact: true });
  await refund.click();
  assert.equal((await read()).funds, cash);
  assert.deepEqual(await shopBounds(), beforePurchase);
  await buy.click();
  await page.locator('#done-shopping').click();
  assert.equal((await read()).status, 0);
  assert.equal(await page.locator('#inventory-panel').evaluate(node => node.parentElement.id), 'combat');
  assert.equal(await page.evaluate(() => document.activeElement.id), 'board');
  await page.keyboard.press('1');
  assert.match(await page.locator('#combat-message').textContent(), /You launched Flip out/i);
  await page.keyboard.press('p');

  const layouts = [];
  for (const [width, height] of [[1920, 1080], [1280, 900], [768, 1024], [390, 844], [320, 568], [844, 390], [568, 320]]) {
    await page.setViewportSize({ width, height });
    await page.evaluate(() => window.scrollTo(0, 0));
    const layout = await page.evaluate(() => {
      const board = document.getElementById('board').getBoundingClientRect();
      const controls = document.querySelector('.play-controls').getBoundingClientRect();
      return { width: innerWidth, height: innerHeight, scrollWidth: document.documentElement.scrollWidth,
        boardWidth: board.width, boardTop: board.top, boardBottom: board.bottom, controlsBottom: controls.bottom };
    });
    assert(layout.scrollWidth <= width, JSON.stringify(layout));
    assert(layout.controlsBottom <= height, JSON.stringify(layout));
    assert(layout.boardBottom <= height, "Board extends below viewport: " + JSON.stringify(layout));
    if (width >= 1280) assert(layout.boardWidth >= 240, "Desktop board is too small");
    layouts.push(layout);
  }
  await settings();
  await page.locator('#mode').selectOption('0');
  await page.locator('#restart').click();
  assert(await page.locator('#opponent-panel').isHidden());
  assert(await page.locator('#combat').isHidden());
  await advance(400);
  await page.keyboard.press('p');
  const beforeResume = (await read()).cells;
  await advance(2000);
  await page.keyboard.press('p');
  await advance(500);
  assert.deepEqual((await read()).cells, beforeResume);
  await advance(20);
  assert.notDeepEqual((await read()).cells, beforeResume);
  const touchBefore = await read();
  await page.locator('[data-command="4"]').click();
  assert((await read()).score > touchBefore.score);
  assert.equal(await page.evaluate(() => document.activeElement.id), 'board');
  await page.keyboard.press('p');
  await settings();
  await page.locator('#mode').selectOption('1');
  await page.locator('#restart').click();
  for (let i = 0; i < 100 && (await read()).status === 0; ++i)
    { await page.keyboard.press('Space'); await advance(450); }
  assert.equal((await read()).status, 2);
  assert.match(await page.locator('#status').textContent(), /You suck!/);
  assert(await page.locator('#pause').isDisabled());
  assert(await page.locator('#opponent-board').isHidden());
  await restart();
  assert.equal((await read()).status, 0);
  assert.equal((await read()).score, 0);
  await page.keyboard.press('p');
  assert.deepEqual(errors, []);
  await page.unroute('**/app*.js', intercept);
  return { passed: 'Canvas, keyboard, focus, pause/blur, Condor, earned-funds shopping/refund/launch, responsive layout, solo buttons', layouts };
}
