"use strict";

// Palette and 23-pixel tiles follow BattleTris.C and BTBox.C.
const BattleTrisUI = (() => {
  const colors = ['#000000', '#eeeee0', '#eeee00', '#ee0000', '#0000cd', '#ee9a00', '#32cd32', '#009acd', '#a020f0', '#bfbfbf'];
  const shadows = ['#bfbfbf', '#a8a8a8', '#daa520', '#8b0000', '#00008b', '#da7600', '#228b22', '#436eee', '#68228b'];
  const updateDensity = () => document.documentElement.style.setProperty('--pixel-ratio', window.devicePixelRatio || 1);
  updateDensity();
  window.addEventListener('resize', updateDensity);

  function drawBoard(canvas, cells, gimp) {
    const displayed = canvas.getBoundingClientRect().width;
    const tile = Math.max(1, Math.round((displayed ? displayed * (window.devicePixelRatio || 1) : canvas.width) / 10));
    if (canvas.width !== tile * 10 || canvas.height !== tile * 28) {
      canvas.width = tile * 10; canvas.height = tile * 28;
    }
    const scale = tile / 23;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.save();
    ctx.imageSmoothingEnabled = false;
    for (let i = 0; i < 280; ++i) {
      const id = cells[i];
      if (id <= 0) continue;
      ctx.save();
      ctx.translate((i % 10) * tile, Math.floor(i / 10) * tile);
      const color = id === 21 || id === 22 ? 2 : id >= 24 ? 1 : id;
      ctx.fillStyle = shadows[color] || colors[9];
      ctx.fillRect(0, 0, tile, tile);
      ctx.fillStyle = colors[color] || colors[9];
      ctx.fillRect(0, 0, tile - Math.round(3 * scale), tile - Math.round(3 * scale));
      if (id >= 24 && id <= 29) {
        const n = id - 23, pips = [];
        if (n > 1) pips.push([1, 1], [13, 13]);
        if (n > 3) pips.push([13, 1], [1, 13]);
        if (n % 2) pips.push([7, 7]);
        if (n === 6) pips.push([1, 7], [13, 7]);
        for (const [x, y] of pips) {
          const border = Math.max(1, Math.round(scale)), inside = Math.max(1, Math.round(3 * scale));
          const px = Math.round(x * scale), py = Math.round(y * scale);
          ctx.fillStyle = shadows[1]; ctx.fillRect(px, py, inside + 2 * border, inside + 2 * border);
          ctx.fillStyle = '#000'; ctx.fillRect(px + border, py + border, inside, inside);
        }
      } else if (id === 23 && gimp.complete && gimp.naturalWidth) {
        ctx.drawImage(gimp, 0, 0, tile, tile);
      } else if (id === 21 || id === 22) {
        ctx.save(); ctx.scale(scale, scale);
        ctx.fillStyle = '#000';
        for (const x of [4, 13]) {
          ctx.beginPath(); ctx.ellipse(x, 4.5, 2, 3.5, 0, 0, 2 * Math.PI); ctx.fill();
        }
        ctx.strokeStyle = '#000';
        ctx.beginPath();
        ctx.ellipse(8.5, id === 21 ? 10.5 : 15.5, 5.5, 2.5, 0, 0, Math.PI, id === 22);
        ctx.stroke();
        if (id === 22) { ctx.fillStyle = colors[4]; ctx.fillRect(12, 8, 2, 3); }
        ctx.restore();
      }
      ctx.restore();
    }
    ctx.restore();
  }

  function text(node, value) {
    if (node.textContent !== value) node.textContent = value;
  }
  function shop(weapons, purchase) {
    const container = document.getElementById('shop');
    const list = document.createElement('select');
    list.className = 'weapon-list'; list.size = 14;
    list.setAttribute('aria-label', 'Weapons for sale');
    const price = document.createElement('p'), buy = document.createElement('button');
    const remove = document.createElement('button');
    buy.textContent = 'Add >>'; remove.textContent = '<< Remove';
    const descriptions = document.createElement('div'); descriptions.className = 'weapon-descriptions';
    const rows = [...weapons].sort((a, b) => a.price - b.price).map(weapon => {
      const option = document.createElement('option');
      option.value = weapon.token; list.append(option);
      const description = document.createElement('p');
      description.textContent = weapon.description; descriptions.append(description);
      return { weapon, option, description, disabled: true };
    });
    let selectedSlot = 0;
    const inventory = document.getElementById('arsenal');
    inventory.addEventListener('click', event => {
      const label = event.target.closest('.slot > button:first-child');
      if (label) { selectedSlot = [...inventory.children].indexOf(label.parentElement); refresh(); }
    });
    function refresh() {
      for (const row of rows) {
        text(row.option, row.weapon.name + ' - $' + row.weapon.price);
        const selected = row.option.index === list.selectedIndex;
        row.description.style.visibility = selected ? 'visible' : 'hidden';
        row.description.setAttribute('aria-hidden', String(!selected));
      }
      const row = rows[list.selectedIndex];
      if (!row) return;
      const weapon = row.weapon;
      text(price, 'Price:    ' + weapon.price + '\nDuration: ' + weapon.duration + ' lines');
      buy.setAttribute('aria-label', 'Buy ' + weapon.name); buy.disabled = row.disabled;
      [...inventory.children].forEach((slot, index) => {
        slot.firstElementChild.setAttribute('aria-pressed', String(index === selectedSlot));
      });
      const refund = inventory.children[selectedSlot]?.lastElementChild;
      remove.disabled = !refund || refund.hidden || refund.disabled;
      remove.setAttribute('aria-label', 'Undo purchase from arsenal slot ' + ((selectedSlot + 1) % 10));
    }
    list.selectedIndex = 0; list.addEventListener('change', refresh);
    buy.addEventListener('click', () => purchase(rows[list.selectedIndex].weapon.token));
    remove.addEventListener('click', () => inventory.children[selectedSlot].lastElementChild.click());
    const actions = document.createElement('div'); actions.className = 'shop-actions';
    actions.append(document.querySelector('.shop-funds-box'), buy, remove, document.getElementById('done-shopping'));
    const info = document.createElement('div'); info.className = 'weapon-info';
    info.append(price, descriptions);
    container.replaceChildren(list, actions, document.getElementById('shop-inventory'), info);
    refresh();
    return { rows, refresh };
  }
  const about = document.createElement('dialog');
  about.className = 'about-dialog'; about.setAttribute('aria-labelledby', 'about-title');
  about.innerHTML = `
    <h1 id="about-title">BattleTris</h1>
    <div class="about-authors">
      <img src="assets/shield.png" alt="" width="99" height="105">
      <div><p class="about-version">Version 1.0</p>
      <p>Bryan Cantrill<br>Charlie Hoecker<br>Mike Shapiro</p>
      <p class="about-email">battletris@cs.brown.edu</p></div>
      <img src="assets/shield.png" alt="" width="99" height="105">
    </div>
    <div class="about-thanks">
      <p>BattleTris Copyright (c) 1993-1997 Bryan Cantrill, Charles Hoecker, Michael Shapiro.</p>
      <h2>Special thanks to:</h2>
      <p>Libby "Hoss the Camel" Cantrill, for many ideas and extensive play-testing</p>
      <p>Drew Davis, for great advice early on</p>
      <p>Tony, for cleaning up our empty Mountain Dew bottles</p>
      <p>botrytis, pebbles and barney for many long and passionate nights</p>
      <p>The original BT beta testers: Ben, Caffer, Masi, Dave, Scott and Todd</p>
      <p>and of course</p>
      <p>Kevin "shouldn't there be a paren there?" Regan</p>
    </div>
    <form method="dialog"><button>OK</button></form>`;
  document.body.append(about);
  const aboutButton = document.createElement('button'); aboutButton.textContent = 'About';
  aboutButton.addEventListener('click', () => {
    document.dispatchEvent(new Event('battletris:about'));
    about.showModal();
  });
  document.querySelector('nav').append(aboutButton);
  return { drawBoard, shop };
})();
