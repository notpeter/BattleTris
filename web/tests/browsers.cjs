// NODE_PATH can point to an existing Playwright installation.
// node web/tests/browsers.cjs [chromium firefox webkit]
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const playwright = require('playwright');
const checks = eval('(' + fs.readFileSync(path.join(__dirname, 'browser.js'), 'utf8') + ')');
const requested = process.argv.slice(2);
const baseURL = process.env.BASE_URL || 'http://127.0.0.1:8000/';

(async () => {
  fs.mkdirSync('.playwright-mcp', { recursive: true });
  for (const name of requested.length ? requested : ['chromium', 'firefox', 'webkit']) {
    assert(['chromium', 'firefox', 'webkit'].includes(name), 'Unknown browser: ' + name);
    const browser = await playwright[name].launch({ headless: true });
    try {
      const context = await browser.newContext();
      const result = await checks(await context.newPage(), baseURL);
      await context.close();
      const mobile = await browser.newContext({ viewport: { width: 390, height: 844 },
        hasTouch: true, isMobile: name !== 'firefox', deviceScaleFactor: 2 });
      const page = await mobile.newPage();
      const errors = [];
      page.on('pageerror', error => errors.push(error.message));
      await page.goto(baseURL);
      await page.waitForFunction(() => document.querySelector('#status').textContent.includes('playing'));
      assert.match(await page.locator('#status').textContent(), /playing/);
      await page.locator('[data-command="0"]').tap();
      await page.locator('[data-command="2"]').tap();
      await page.locator('[data-command="4"]').tap();
      assert(Number(await page.locator('#score').textContent()) > 0);
      await page.locator('#pause').tap();
      const pixels = await page.locator('#board').evaluate(canvas => canvas.toDataURL());
      await page.waitForTimeout(300);
      assert.equal(await page.locator('#board').evaluate(canvas => canvas.toDataURL()), pixels);
      await page.locator('#toggle-recon').tap();
      assert.equal(await page.locator('#toggle-recon').getAttribute('aria-pressed'), 'false');
      assert(await page.locator('#opponent-panel').isHidden());
      await page.evaluate(() => window.scrollTo(0, 0));
      const bounds = await page.locator('.play-controls').boundingBox();
      assert(bounds.y + bounds.height <= 844, 'Touch controls extend below viewport');
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true);
      assert.deepEqual(errors, []);
      await page.screenshot({ path: `.playwright-mcp/${name}-touch.png` });
      await page.setViewportSize({ width: 844, height: 390 });
      await page.evaluate(() => window.scrollTo(0, 0));
      const landscape = await page.locator('.play-controls').boundingBox();
      assert(landscape.y + landscape.height <= 390, 'Landscape controls extend below viewport');
      await page.screenshot({ path: `.playwright-mcp/${name}-landscape.png` });
      await mobile.close();
      // Check missing script/WASM/image failures and recovery on real pages.
      for (const pattern of ['**/app*.js', '**/ui*.js', '**/*.wasm', '**/gimp*.png']) {
        const broken = await browser.newContext();
        const failedPage = await broken.newPage();
        await failedPage.route(pattern, route => route.abort());
        await failedPage.goto(baseURL);
        await failedPage.locator('#load-error').waitFor({ state: 'visible' });
        assert(await failedPage.locator('#pause').isDisabled());
        await failedPage.unroute(pattern);
        await failedPage.locator('#retry-load').click();
        await failedPage.waitForFunction(() => document.querySelector('#status').textContent.includes('playing'));
        assert(await failedPage.locator('#load-error').isHidden());
        await broken.close();
      }
      console.log(JSON.stringify({ browser: name, version: browser.version(), ...result,
        touch: 'emulated taps passed', recovery: 'missing script, WASM, and artwork retry passed' }));
    } finally { await browser.close(); }
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
