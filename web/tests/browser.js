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
  const read = () => page.evaluate(() => {
    const g = window.__game;
    const p = g._bt_cells() >>> 2;
    return { status: g._bt_status(), score: g._bt_score(), funds: g._bt_funds(),
      opScore: g._bt_op_score(), cells: Array.from(g.HEAP32.subarray(p, p + 280)) };
  });
  const advance = ms => page.evaluate(ms => window.__advance(ms), ms);
  const settings = async () => { await page.locator('.settings').evaluate(node => { node.open = true; }); };
  const restart = async () => { await settings(); await page.locator('#restart').click(); };
  assert.equal((await read()).status, 1);
  assert(await page.locator('#opponent-board').isHidden());
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
  await page.locator('#level').selectOption('2');
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
  await page.keyboard.press('c');
  assert(await page.locator('#opponent-board').isHidden());
  assert.equal(await page.locator('#op-funds').textContent(), '?');
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
  const earn = [1,1,1,4,1,4,0,0,4,0,0,0,4,0,0,0,0,4,0,0,0,0,0,4,0,0,4,
    0,0,0,0,0,4,2,2,2,1,4,2,1,1,1,1,1,4,1,1,4,0,0,0,4,0,0,4,4,1,4,1,1,4,
    2,2,0,0,0,0,0,4];
  const keys = ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Space'];
  for (const command of earn) await page.keyboard.press(keys[command]);
  assert.equal((await read()).funds, 30);
  for (let i = 0; i < 500 && (await read()).status === 0; ++i) await advance(100);
  assert.equal((await read()).status, 5);
  assert.equal(await page.evaluate(() => document.activeElement.id), 'bazaar');
  assert.equal(await page.locator('#shop .weapon').count(), 34);
  assert.equal(await page.locator('#inventory-panel').evaluate(node => node.parentElement.id), 'shop-inventory');
  const frozen = await read();
  await advance(2000);
  assert.deepEqual(await read(), frozen);
  const buy = page.getByRole('button', { name: /^Buy Flip out$/i });
  const cash = (await read()).funds;
  await buy.click();
  assert.equal((await read()).funds, cash - 15);
  const refund = page.getByRole('button', { name: 'Undo purchase from arsenal slot 1', exact: true });
  await refund.click();
  assert.equal((await read()).funds, cash);
  await buy.click();
  await page.locator('#done-shopping').click();
  assert.equal((await read()).status, 0);
  assert.equal(await page.locator('#inventory-panel').evaluate(node => node.parentElement.id), 'combat');
  assert.equal(await page.evaluate(() => document.activeElement.id), 'board');
  await page.keyboard.press('1');
  assert.match(await page.locator('#combat-message').textContent(), /You launched Flip out/i);
  await page.keyboard.press('p');

  const layouts = [];
  for (const [width, height] of [[1280, 900], [768, 1024], [390, 844], [320, 568], [844, 390], [568, 320]]) {
    await page.setViewportSize({ width, height });
    await page.evaluate(() => window.scrollTo(0, 0));
    const layout = await page.evaluate(() => {
      const board = document.getElementById('board').getBoundingClientRect();
      const controls = document.querySelector('.play-controls').getBoundingClientRect();
      return { width: innerWidth, height: innerHeight, scrollWidth: document.documentElement.scrollWidth,
        boardTop: board.top, boardBottom: board.bottom, controlsBottom: controls.bottom };
    });
    assert(layout.scrollWidth <= width, JSON.stringify(layout));
    assert(layout.controlsBottom <= height, JSON.stringify(layout));
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
    await page.keyboard.press('Space');
  assert.equal((await read()).status, 2);
  assert.match(await page.locator('#status').textContent(), /Ernie wins/);
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
