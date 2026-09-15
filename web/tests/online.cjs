// Run after `make -C web`: NODE_PATH=<playwright install>/node_modules node web/tests/online.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const { chromium } = require('playwright');
const { createService } = require('../../server/index.cjs');

(async () => {
  const service = await createService({ graceMs: 3000, pauseMs: 5000,
    seedFactory: () => 42 });
  const address = await service.listen(0, '127.0.0.1');
  const url = `http://127.0.0.1:${address.port}/online.html`;
  const browser = await chromium.launch({ headless: true });
  try {
    const contexts = await Promise.all([browser.newContext(), browser.newContext()]);
    const [host, guest] = await Promise.all(contexts.map(context => context.newPage()));
    const errors = [];
    for (const page of [host, guest]) page.on('pageerror', error => errors.push(error.message));
    await host.goto(url);
    await host.locator('#create-room').click();
    await host.locator('#invite-link').waitFor({ state: 'visible' });
    const invitation = await host.locator('#invite-link').textContent();
    await guest.goto(invitation.trim());
    await guest.locator('#ready').waitFor({ state: 'visible' });
    await host.locator('#ready').click();
    await guest.locator('#ready').click();
    for (const page of [host, guest]) {
      await page.waitForFunction(() => !document.querySelector('[data-command="4"]').disabled);
      assert.equal(await page.locator('#op-funds').textContent(), 'Unknown');
      assert(await page.locator('#opponent-board').isHidden());
    }
    await host.locator('#board').focus();
    await host.keyboard.press('ArrowLeft');
    await host.keyboard.press('Space');
    await host.waitForFunction(() => Number(document.querySelector('#score').textContent) > 0);
    await guest.locator('[data-command="4"]').click();
    await guest.waitForFunction(() => Number(document.querySelector('#score').textContent) > 0);
    await host.locator('#pause').click();
    await guest.waitForFunction(() => /accept/i.test(document.querySelector('#pause').textContent));
    await guest.locator('#pause').click();
    for (const page of [host, guest]) {
      await page.waitForFunction(() => /paused/i.test(document.querySelector('#status').textContent));
    }
    const frozen = await host.locator('#board').evaluate(canvas => canvas.toDataURL());
    await host.waitForTimeout(200);
    assert.equal(await host.locator('#board').evaluate(canvas => canvas.toDataURL()), frozen);
    fs.mkdirSync('.playwright-mcp', { recursive: true });
    for (const [width, height] of [[390, 844], [844, 390]]) {
      await host.setViewportSize({ width, height });
      await host.evaluate(() => scrollTo(0, 0));
      const controls = await host.locator('.play-controls').boundingBox();
      assert(controls.y + controls.height <= height, 'Online controls extend below viewport');
      assert(await host.evaluate(() => document.documentElement.scrollWidth <= innerWidth),
        'Online page overflows horizontally');
      await host.screenshot({ path: `.playwright-mcp/online-${width}x${height}.png` });
    }
    await host.locator('#pause').click();
    await host.waitForFunction(() => !document.querySelector('[data-command="4"]').disabled);
    const score = await host.locator('#score').textContent();
    await host.reload();
    await host.waitForFunction(() => !document.querySelector('[data-command="4"]').disabled);
    assert.equal(await host.locator('#score').textContent(), score, 'Reload reconnects to the same seat');
    guest.once('dialog', dialog => dialog.accept());
    await guest.locator('#surrender').click();
    for (const page of [host, guest]) await page.locator('#new-room').waitFor({ state: 'visible' });
    assert.match(await host.locator('#status').textContent(), /win|won/i);
    assert.match(await guest.locator('#status').textContent(), /lose|lost/i);
    // The multiplayer service also hosts offline play. Its CSP must permit
    // the offline startup/error guard and WASM compilation.
    const offline = await contexts[0].newPage();
    offline.on('pageerror', error => errors.push(error.message));
    await offline.goto(url.replace('online.html', 'index.html'));
    await offline.waitForFunction(() => !document.querySelector('#restart').disabled);
    await offline.route('**/app.js', route => route.abort());
    await offline.reload();
    await offline.locator('#load-error').waitFor({ state: 'visible' });
    await offline.close();
    assert.deepEqual(errors, []);
    console.log('Online browser: invitations, two human inputs, private recon, mutual pause, reload reconnect, and surrender passed');
    await Promise.all(contexts.map(context => context.close()));
  } finally {
    await browser.close();
    await service.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
