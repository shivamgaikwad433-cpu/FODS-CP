// Network Failure Analyzer - web front end
// All graph work (BFS, DFS, Dijkstra, Tarjan, ranking) happens in the C++ server.
// This file only draws the map and sends actions to /api/...

(() => {
  'use strict';

  // ---------- palette ----------
  const C = {
    k: '#2a1e14', n: '#1d3557', s: '#4f8fc0', l: '#a9d1ee', o: '#e2702a', a: '#f2a541',
    b: '#f7ecd4', w: '#fff8e8', d: '#c9a97a', g: '#8b7f70', r: '#b8381e',
    sand: '#ebd9b4', sand2: '#e3cda2', parchment: '#f7ecd4'
  };
  // colours used when a device is down: everything goes grey and dull
  const DEAD = { k: '#2a1e14', n: '#4a4440', s: '#7a7068', l: '#9a9088', o: '#6e5a44', a: '#7d6b55', b: '#a89c88', w: '#b8ad9a', d: '#8b7f70', g: '#6b625a', r: '#b8381e' };

  // ---------- pixel icons (24 x 20 grid, drawn with rectangles) ----------
  const IW = 24, IH = 20;
  const X = {               // extra shades for the icons
    N: '#142640', S: '#3a76a8', D: '#a8875a', O: '#b8561c', G: '#cfc3ad'
  };
  const col = c => C[c] || X[c] || c;

  // each icon is a function that gets R(x, y, w, h, colour) and box(x, y, w, h, fill)
  const ICONS = {
    router(R, box) {
      R(5, 2, 1, 6, 'k'); R(18, 2, 1, 6, 'k');
      R(4, 0, 3, 2, 'o'); R(17, 0, 3, 2, 'o'); R(5, 0, 1, 1, 'a'); R(18, 0, 1, 1, 'a');
      box(1, 7, 22, 10, 's');
      R(2, 8, 20, 2, 'l');
      R(2, 15, 20, 1, 'S');
      R(4, 11, 2, 2, 'a'); R(7, 11, 2, 2, 'a'); R(10, 11, 2, 2, 'o');
      for (let i = 0; i < 4; i++) R(14 + i * 2, 11, 1, 3, 'n');
      R(3, 17, 3, 2, 'k'); R(18, 17, 3, 2, 'k');
    },
    switch(R, box) {
      box(0, 7, 24, 10, 's');
      R(1, 8, 22, 2, 'l');
      R(1, 15, 22, 1, 'S');
      for (let i = 0; i < 6; i++) {
        R(2 + i * 3, 11, 2, 3, 'k');
        R(2 + i * 3, 11, 2, 1, i % 2 ? 'o' : 'a');
      }
      R(20, 11, 2, 3, 'n');
      R(2, 17, 3, 2, 'k'); R(19, 17, 3, 2, 'k');
    },
    server(R, box) {
      box(5, 0, 14, 20, 'n');
      for (let j = 0; j < 3; j++) {
        const y = 2 + j * 6;
        R(7, y, 10, 4, 'S');
        R(7, y, 10, 1, 'l');
        R(8, y + 2, 2, 1, 'a'); R(11, y + 2, 1, 1, 'o');
        R(13, y + 2, 3, 1, 'k');
      }
    },
    pc(R, box) {
      box(2, 0, 20, 14, 'b');
      box(4, 2, 16, 10, 'n');
      R(5, 3, 14, 8, 's');
      R(6, 4, 4, 1, 'l'); R(6, 5, 2, 1, 'l');
      R(6, 8, 5, 1, 'a'); R(12, 8, 1, 1, 'w');
      R(17, 12, 2, 1, 'o');
      R(10, 14, 4, 2, 'k');
      box(6, 16, 12, 4, 'D');
      R(7, 17, 10, 1, 'd');
    },
    printer(R, box) {
      box(6, 0, 12, 7, 'w');
      R(8, 2, 8, 1, 'G'); R(8, 4, 6, 1, 'G');
      box(1, 6, 22, 10, 'b');
      R(2, 7, 20, 2, 'd');
      R(14, 10, 2, 2, 'a'); R(17, 10, 2, 2, 'o');
      R(4, 12, 16, 2, 'k');
      box(5, 14, 14, 6, 'w');
      R(7, 16, 10, 1, 'G'); R(7, 18, 6, 1, 'G');
    },
    ap(R, box) {
      R(7, 1, 10, 1, 'l'); R(4, 2, 3, 1, 'l'); R(17, 2, 3, 1, 'l');
      R(2, 3, 2, 1, 'l'); R(20, 3, 2, 1, 'l'); R(1, 4, 1, 2, 'l'); R(22, 4, 1, 2, 'l');
      R(8, 4, 8, 1, 's'); R(6, 5, 2, 1, 's'); R(16, 5, 2, 1, 's'); R(5, 6, 1, 2, 's'); R(18, 6, 1, 2, 's');
      R(10, 7, 4, 1, 'n'); R(9, 8, 1, 1, 'n'); R(14, 8, 1, 1, 'n');
      R(11, 9, 2, 5, 'k');
      box(5, 14, 14, 6, 'b');
      R(6, 15, 12, 1, 'w');
      R(9, 17, 2, 1, 'o'); R(13, 17, 2, 1, 'a');
    },
    firewall(R, box) {
      R(10, 0, 2, 2, 'a'); R(9, 2, 5, 2, 'o'); R(8, 4, 8, 3, 'o'); R(10, 3, 3, 4, 'a'); R(11, 5, 1, 2, 'w');
      box(1, 6, 22, 14, 'D');
      for (let r = 0; r < 4; r++) {
        const y = 7 + r * 3, off = r % 2 ? 3 : 0;
        for (let x = 2 - off; x < 22; x += 6) {
          const x1 = Math.max(2, x), x2 = Math.min(22, x + 5);
          if (x2 <= x1) continue;
          R(x1, y, x2 - x1, 2, 'o');
          R(x1, y, x2 - x1, 1, 'a');
        }
      }
    }
  };

  const BOT = [
    '....ka......',
    '....kk......',
    '..kkkkkkkk..',
    '.kssssssssk.',
    '.kskasskask.',
    '.kssssssssk.',
    '.ksskkkkssk.',
    '.kssssssssk.',
    '..kkkkkkkk..',
    '...knnnnk...',
    '..knnoonnk..',
    '..kkkkkkkk..'
  ];

  const spriteCache = {};
  function sprite(type, dead) {
    const key = type + (dead ? '_dead' : '');
    if (spriteCache[key]) return spriteCache[key];
    const cv = document.createElement('canvas');
    cv.width = IW; cv.height = IH;
    const g = cv.getContext('2d');
    const R = (x, y, w, h, c) => { g.fillStyle = col(c); g.fillRect(x, y, w, h); };
    const box = (x, y, w, h, c) => { R(x, y, w, h, 'k'); R(x + 1, y + 1, w - 2, h - 2, c); };
    (ICONS[type] || ICONS.pc)(R, box);
    if (dead) {
      // grey it out: keep the shape, drain the colour
      const img = g.getImageData(0, 0, IW, IH), px = img.data;
      for (let i = 0; i < px.length; i += 4) {
        if (!px[i + 3]) continue;
        const l = 0.3 * px[i] + 0.59 * px[i + 1] + 0.11 * px[i + 2];
        px[i] = l * 0.55 + 45; px[i + 1] = l * 0.5 + 38; px[i + 2] = l * 0.45 + 32;
      }
      g.putImageData(img, 0, 0);
    }
    spriteCache[key] = cv;
    return cv;
  }

  function drawPixels(canvas, rows, scale) {
    const g = canvas.getContext('2d');
    g.clearRect(0, 0, canvas.width, canvas.height);
    rows.forEach((row, y) => {
      for (let x = 0; x < row.length; x++) {
        const ch = row[x];
        if (!C[ch] || ch === '.') continue;
        g.fillStyle = C[ch];
        g.fillRect(x * scale, y * scale, scale, scale);
      }
    });
  }

  // ---------- state ----------
  let S = null;              // last state from server
  let sel = null;            // {kind:'device', name} or {kind:'link', a, b}
  let route = null;          // {path:[names], cost}
  let reach = null;          // {from, reachable:Set}
  let hover = null;
  let mainGroup = null;      // group id of the biggest connected group
  let untangleOn = false;   // untangle mode button
  let plan = [];             // planned maintenance: [{a, b}] in order
  let planBreak = null;      // index in plan where the network splits
  let planFile = null;
  let lastShape = '';
  let soundOn = true;
  const grid = {};           // name -> {x, y} grid cell
  const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  const $ = s => document.querySelector(s);
  const $$ = s => Array.from(document.querySelectorAll(s));
  const esc = s => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

  // ---------- sound (tiny 8-bit beeps) ----------
  let audio = null;
  function beep(notes) {
    if (!soundOn) return;
    try {
      audio = audio || new (window.AudioContext || window.webkitAudioContext)();
      let t = audio.currentTime;
      notes.forEach(([f, d]) => {
        const o = audio.createOscillator(), v = audio.createGain();
        o.type = 'square';
        o.frequency.value = f;
        v.gain.setValueAtTime(0.05, t);
        v.gain.exponentialRampToValueAtTime(0.001, t + d);
        o.connect(v); v.connect(audio.destination);
        o.start(t); o.stop(t + d);
        t += d * 0.9;
      });
    } catch (e) { /* no audio, fine */ }
  }
  const SFX = {
    select: () => beep([[880, 0.05]]),
    fail: () => beep([[440, 0.08], [330, 0.08], [196, 0.18]]),
    fix: () => beep([[392, 0.07], [523, 0.07], [784, 0.14]]),
    route: () => beep([[659, 0.06], [880, 0.06], [988, 0.1]]),
    error: () => beep([[150, 0.15]])
  };

  // ---------- server calls ----------
  async function api(path, params) {
    // GitHub Pages version: the C++ analyzer runs as WebAssembly inside the page
    if (window.NFA_READY) {
      await window.NFA_READY;
      return JSON.parse(window.NFA_WASM('/api/' + path, params ? new URLSearchParams(params).toString() : ''));
    }
    const q = params ? '?' + new URLSearchParams(params).toString() : '';
    const res = await fetch('/api/' + path + q);
    if (!res.ok && res.status !== 200) throw new Error('Server answered ' + res.status);
    return res.json();
  }

  async function act(path, params, okSound) {
    try {
      const data = await api(path, params);
      if (data.devices) setState(data);
      if (data.msg) say(cleanMsg(data.msg));
      (data.ok ? (okSound || SFX.select) : SFX.error)();
      return data;
    } catch (e) {
      offline();
    }
  }

  function cleanMsg(m) { return m.replace(/^\[\d\d:\d\d:\d\d\]\s*/, ''); }

  function offline() {
    say(window.NFA_READY
      ? 'The WebAssembly analyzer did not load. Refresh the page, or try another browser.'
      : 'I can\'t reach the C++ server. Run run.bat (or server.exe from the project folder), then refresh this page.');
    $('#net-name').textContent = 'Server offline';
  }

  // ---------- dialog box with typewriter text ----------
  let typeTimer = null;
  function say(text) {
    const el = $('#dialog-text');
    clearInterval(typeTimer);
    if (reduceMotion) { el.textContent = text; return; }
    let i = 0;
    el.textContent = '';
    typeTimer = setInterval(() => {
      i += 2;
      el.textContent = text.slice(0, i);
      if (i >= text.length) clearInterval(typeTimer);
    }, 16);
  }

  // ---------- state -> screen ----------
  function setState(data) {
    const prevFile = S && S.file;
    S = data;
    if (prevFile && prevFile !== S.file) { sel = null; route = null; reach = null; }

    // keep positions we already know, place the rest on free cells
    const used = new Set();
    S.devices.forEach(d => {
      if (d.x >= 0 && d.y >= 0) { grid[d.name] = { x: d.x, y: d.y }; used.add(d.x + ',' + d.y); }
    });
    S.devices.forEach(d => {
      if (d.x < 0 || d.y < 0) {
        if (!grid[d.name]) grid[d.name] = freeCell(used);
        used.add(grid[d.name].x + ',' + grid[d.name].y);
      }
    });

    // untangle mode: re-arrange whenever a different network or a new device/cable shows up
    const shape = S.file + ':' + S.devices.length + ':' + S.links.length;
    if (untangleOn && shape !== lastShape) untangle(false);
    lastShape = shape;

    // selected thing might not exist any more
    if (sel && sel.kind === 'device' && !findDevice(sel.name)) sel = null;
    if (route && !route.path.every(n => findDevice(n) && findDevice(n).working)) route = null;
    reach = null;

    // the biggest group is the "main" network, others are cut off
    const count = {};
    S.devices.forEach(d => { if (d.group >= 0) count[d.group] = (count[d.group] || 0) + 1; });
    const groupIds = Object.keys(count);
    mainGroup = groupIds.length > 1 ? +groupIds.sort((a, b) => count[b] - count[a])[0] : null;

    renderHud();
    renderSelected();
    renderIntel();
    renderLog();
    fillDeviceSelects();
    layoutMap();
  }

  function freeCell(used) {
    const b = bounds();
    for (let y = b.maxY + 1; y < b.maxY + 4; y++)
      for (let x = b.minX; x <= b.maxX; x++)
        if (!used.has(x + ',' + y)) return { x, y };
    return { x: b.maxX + 1, y: b.maxY };
  }

  const findDevice = name => S && S.devices.find(d => d.name === name);
  const findLink = (a, b) => S && S.links.find(l => (l.a === a && l.b === b) || (l.a === b && l.b === a));

  function renderHud() {
    const st = S.stats;
    $('#net-name').textContent = S.name;
    $('#st-dev').textContent = st.devicesUp + '/' + st.devices;
    $('#st-link').textContent = st.linksUp + '/' + st.links;
    $('#st-groups').textContent = st.groups;
    $('#st-cut').textContent = st.cut;
    const sev = $('#st-sev');
    sev.textContent = st.severity.charAt(0) + st.severity.slice(1).toLowerCase();
    sev.className = 'sev ' + st.severity;

    const bar = $('#hp-bar');
    const on = Math.round(st.health / 5);
    bar.innerHTML = Array.from({ length: 20 }, (_, i) => `<i class="${i < on ? 'on' : ''}"></i>`).join('');
    bar.className = 'hp-bar' + (st.health < 50 ? ' bad' : st.health < 85 ? ' warn' : '');
    bar.setAttribute('aria-valuenow', st.health);
    $('#hp-num').textContent = Math.round(st.health) + '%';
    $('#undo-btn').disabled = !S.canUndo;
  }

  function renderSelected() {
    const body = $('#selected-body');
    if (!sel) {
      body.className = 'muted';
      body.textContent = 'Click a device or a cable on the map.';
      return;
    }
    body.className = '';
    if (sel.kind === 'device') {
      const d = findDevice(sel.name);
      const rank = S.ranking.find(r => r.name === d.name);
      body.innerHTML = `
        <div class="sel-head"><canvas width="24" height="20" id="sel-sprite"></canvas>
          <div><div class="sel-name">${esc(d.name)}</div>
          <span class="tag">${esc(typeName(d.type))}</span>
          <span class="tag ${d.working ? '' : 'down'}">${d.working ? 'Online' : 'Down'}</span>
          ${d.critical ? '<span class="tag crit">Single point of failure</span>' : ''}
          <span class="tag">${d.degree} working cable${d.degree === 1 ? '' : 's'}</span></div>
        </div>
        <p class="muted">${d.working
          ? (rank && rank.lost ? `If this fails, ${rank.lost} other device${rank.lost > 1 ? 's' : ''} lose connection.` : 'Other devices can work without this one.')
          : 'This device is down. Cables to it carry nothing.'}</p>
        <div class="row">
          ${d.working ? '<button class="btn danger" data-do="fail">Fail device</button>' : '<button class="btn" data-do="fix">Repair device</button>'}
          <button class="btn sky" data-do="reach" ${d.working ? '' : 'disabled'}>Show reach (BFS)</button>
          <button class="btn sky" data-do="route-from">Route from here</button>
        </div>`;
      const sc = $('#sel-sprite');
      sc.getContext('2d').drawImage(sprite(d.type, !d.working), 0, 0);
    } else {
      const l = findLink(sel.a, sel.b);
      body.innerHTML = `
        <div class="sel-name">${esc(l.a)} ↔ ${esc(l.b)}</div>
        <p><span class="tag">${l.latency} ms</span>
          <span class="tag ${l.working ? '' : 'down'}">${l.working ? (l.usable ? 'Carrying traffic' : 'Idle, an end is down') : 'Cut'}</span>
          ${l.bridge ? '<span class="tag crit">Bridge cable</span>' : ''}</p>
        <div class="row">
          ${l.working ? '<button class="btn danger" data-do="cut">Cut cable</button>' : '<button class="btn" data-do="splice">Repair cable</button>'}
          <button class="btn sky" data-do="plan">Add to maintenance plan</button>
        </div>`;
    }
  }

  function typeName(t) {
    return ({ pc: 'PC', ap: 'Wi-Fi AP', router: 'Router', switch: 'Switch', server: 'Server', printer: 'Printer', firewall: 'Firewall' })[t] || t;
  }

  function renderIntel() {
    const crit = S.devices.filter(d => d.critical);
    $('#crit-list').innerHTML = crit.length
      ? crit.map(d => `<button class="chip" data-pick="${esc(d.name)}">${esc(d.name)}</button>`).join('')
      : '<span class="muted">None. Every device has a backup route.</span>';
    const br = S.links.filter(l => l.bridge);
    $('#bridge-list').innerHTML = br.length
      ? br.map(l => `<button class="chip blue" data-link="${esc(l.a)}|${esc(l.b)}">${esc(l.a)} ↔ ${esc(l.b)}</button>`).join('')
      : '<span class="muted">None.</span>';

    const max = Math.max(1, ...S.ranking.map(r => r.lost));
    $('#rank-list').innerHTML = S.ranking.map(r =>
      `<li data-pick="${esc(r.name)}"><span>${esc(r.name)}</span><span class="bar"><i style="width:${r.lost / max * 100}%"></i></span><b>${r.lost}</b></li>`).join('');

    const groups = {};
    S.devices.forEach(d => { if (d.group >= 0) (groups[d.group] = groups[d.group] || []).push(d.name); });
    const keys = Object.keys(groups);
    $('#group-list').innerHTML = keys.map((k, i) =>
      `<div class="group"><b>Group ${i + 1}</b> (${groups[k].length}): ${groups[k].map(esc).join(', ')}</div>`).join('')
      + (keys.length > 1 ? '<p class="warn-line">The network is split. Devices in different groups cannot talk.</p>' : '');
    const n = S.stats.devicesUp, maxLinks = n * (n - 1) / 2;
    $('#density-line').innerHTML = `<b>Density</b> ${S.stats.density.toFixed(2)}: ${S.stats.linksUp} working cables out of ${maxLinks} possible.`;
    if (S.file !== planFile) { plan = []; planBreak = null; planFile = S.file; $('#plan-result').innerHTML = ''; }
    renderPlan();
  }

  function renderLog() {
    $('#log-list').innerHTML = S.logs.slice().reverse().map(line => {
      const cls = /FAILED/.test(line) ? 'fail' : /RESTORED/.test(line) ? 'fix' : '';
      return `<li class="${cls}">${esc(line)}</li>`;
    }).join('');
  }

  function fillLinkSelect() {
    const s = $('#plan-select');
    s.innerHTML = S.links.map(l => `<option value="${esc(l.a)}:${esc(l.b)}">${esc(l.a)} ↔ ${esc(l.b)}</option>`).join('');
  }

  function fillDeviceSelects() {
    fillLinkSelect();
    const names = S.devices.map(d => d.name);
    $$('.dev-select').forEach((s, i) => {
      const old = s.value;
      s.innerHTML = names.map(n => `<option>${esc(n)}</option>`).join('');
      if (names.includes(old)) s.value = old;
      else if (names.length > 1) s.value = names[(s.name === 'b' || s.name === 'to') ? names.length - 1 : 0];
    });
  }

  // ---------- map drawing ----------
  const cv = $('#map');
  const ctx = cv.getContext('2d');
  let W = 0, H = 0, DPR = 1, cell = 60, cx = 60, cy = 60, ox = 0, oy = 0, scale = 3;
  const pos = {}; // name -> pixel centre
  const shown = {}; // name -> where it is drawn right now (slides towards pos)

  function bounds() {
    const pts = Object.values(grid);
    if (!pts.length) return { minX: 0, maxX: 6, minY: 0, maxY: 4 };
    return {
      minX: Math.min(...pts.map(p => p.x)), maxX: Math.max(...pts.map(p => p.x)),
      minY: Math.min(...pts.map(p => p.y)), maxY: Math.max(...pts.map(p => p.y))
    };
  }

  function layoutMap() {
    const r = cv.getBoundingClientRect();
    DPR = window.devicePixelRatio || 1;
    W = r.width; H = r.height;
    cv.width = Math.round(W * DPR); cv.height = Math.round(H * DPR);
    if (!S) return;
    const b = bounds();
    const cols = b.maxX - b.minX + 1, rows = b.maxY - b.minY + 1;
    const padX = 60, padTop = 56, padBottom = 175; // leave room for dialog box
    // x and y spacing are separate so the map fills the whole box
    cx = Math.max(40, Math.min((W - padX * 2) / Math.max(cols - 1, 1), 150));
    cy = Math.max(44, Math.min((H - padTop - padBottom) / Math.max(rows - 1, 1), 120));
    const m = Math.min(cx * 1.5, cy);
    scale = Math.max(2, Math.min(4, Math.round(Math.min(cx * 0.9 / IW, cy * 0.66 / IH))));
    cell = Math.min(cx, cy);
    ox = (W - (cols - 1) * cx) / 2 - b.minX * cx;
    oy = padTop + ((H - padTop - padBottom) - (rows - 1) * cy) / 2 - b.minY * cy;
    for (const n in grid) pos[n] = { x: ox + grid[n].x * cx, y: oy + grid[n].y * cy };
  }

  function toGrid(px, py) {
    return { x: Math.round((px - ox) / cx), y: Math.round((py - oy) / cy) };
  }

  function drawFloor() {
    const t = 24;
    ctx.fillStyle = C.sand;
    ctx.fillRect(0, 0, W, H);
    // a few darker tiles in a fixed pattern so the floor looks like a game map
    ctx.fillStyle = C.sand2;
    for (let y = 0; y < H; y += t)
      for (let x = 0; x < W; x += t)
        if (((x / t) * 7 + (y / t) * 13) % 11 === 0) ctx.fillRect(x, y, t, t);
    ctx.fillStyle = 'rgba(42,30,20,0.05)';
    for (let y = 0; y < H; y += t) ctx.fillRect(0, y, W, 1);
    for (let x = 0; x < W; x += t) ctx.fillRect(x, 0, 1, H);
  }

  function linkOnRoute(l) {
    if (!route) return false;
    for (let i = 0; i + 1 < route.path.length; i++) {
      const a = route.path[i], b = route.path[i + 1];
      if ((l.a === a && l.b === b) || (l.a === b && l.b === a)) return true;
    }
    return false;
  }

  function drawLinks(t) {
    S.links.forEach(l => {
      const A = shown[l.a], B = shown[l.b];
      if (!A || !B) return;
      const isSel = sel && sel.kind === 'link' && findLink(sel.a, sel.b) === l;
      const isHover = hover && hover.kind === 'link' && hover.l === l;

      ctx.lineCap = 'square';
      if (linkOnRoute(l)) {
        ctx.strokeStyle = C.k; ctx.lineWidth = 12; line(A, B);
        ctx.strokeStyle = C.l; ctx.lineWidth = 8; line(A, B);
      }
      if (!l.working) {
        ctx.setLineDash([8, 6]);
        ctx.strokeStyle = C.r; ctx.lineWidth = 4; line(A, B);
        ctx.setLineDash([]);
        // spark in the middle
        const mx = (A.x + B.x) / 2, my = (A.y + B.y) / 2;
        const f = Math.floor(t * 8) % 2;
        ctx.fillStyle = f ? C.a : C.o;
        ctx.fillRect(mx - 6, my - 2, 12, 4);
        ctx.fillRect(mx - 2, my - 6, 4, 12);
      } else {
        ctx.strokeStyle = C.k; ctx.lineWidth = isSel || isHover ? 8 : 6; line(A, B);
        ctx.strokeStyle = l.usable ? (isSel ? C.o : C.s) : C.g;
        ctx.lineWidth = isSel || isHover ? 4 : 2; line(A, B);
      }

      // latency tag
      const mx = (A.x + B.x) / 2, my = (A.y + B.y) / 2;
      if (l.working && ((cx > 80 && cy > 95) || isSel || isHover)) {
        const txt = l.latency + 'ms';
        ctx.font = '800 14px "Nunito", sans-serif';
        const w = ctx.measureText(txt).width + 8;
        ctx.fillStyle = C.parchment; ctx.fillRect(mx - w / 2, my - 10, w, 20);
        ctx.strokeStyle = C.k; ctx.lineWidth = 2; ctx.strokeRect(mx - w / 2, my - 10, w, 20);
        ctx.fillStyle = l.bridge ? C.o : C.n;
        ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
        ctx.fillText(txt, mx, my + 1);
      }

      // little data packets moving on working links
      if (l.usable && !reduceMotion) {
        const speed = 0.35 / Math.sqrt(l.latency);
        for (let k = 0; k < 2; k++) {
          let p = (t * speed + k * 0.5 + (l.a.length * 0.13)) % 1;
          if (k === 1) p = 1 - p; // one each way
          const x = A.x + (B.x - A.x) * p, y = A.y + (B.y - A.y) * p;
          ctx.fillStyle = C.k; ctx.fillRect(Math.round(x) - 4, Math.round(y) - 4, 8, 8);
          ctx.fillStyle = k ? C.a : C.o; ctx.fillRect(Math.round(x) - 2, Math.round(y) - 2, 4, 4);
        }
      }
    });
  }

  function line(A, B) {
    ctx.beginPath(); ctx.moveTo(A.x, A.y); ctx.lineTo(B.x, B.y); ctx.stroke();
  }

  function drawPlan(t) {
    plan.forEach((c, i) => {
      const A = shown[c.a], B = shown[c.b];
      if (!A || !B) return;
      const isBreak = planBreak === i;
      if (isBreak) {
        ctx.strokeStyle = Math.floor(t * 4) % 2 ? C.r : C.o;
        ctx.lineWidth = 10; ctx.setLineDash([10, 6]); line(A, B); ctx.setLineDash([]);
      }
      // numbered tag one third along the cable
      const x = Math.round(A.x + (B.x - A.x) * 0.33), y = Math.round(A.y + (B.y - A.y) * 0.33);
      ctx.fillStyle = C.k; ctx.fillRect(x - 12, y - 12, 24, 24);
      ctx.fillStyle = isBreak ? C.r : C.a; ctx.fillRect(x - 10, y - 10, 20, 20);
      ctx.fillStyle = isBreak ? C.w : C.k;
      ctx.font = '800 14px "Nunito", sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText(String(i + 1), x, y + 1);
    });
  }

  function addToPlan(a, b) {
    if (plan.some(c => (c.a === a && c.b === b) || (c.a === b && c.b === a))) { say('That cable is already in the plan.'); SFX.error(); return; }
    plan.push({ a, b });
    planBreak = null;
    $('#plan-result').innerHTML = '';
    renderPlan();
    say(`Added ${a} ↔ ${b} as step ${plan.length} of the maintenance plan.`);
    SFX.select();
  }

  function renderPlan() {
    $('#plan-list').innerHTML = plan.map((c, i) =>
      `<li class="${planBreak === i ? 'breaks' : ''}">${esc(c.a)} ↔ ${esc(c.b)}<button data-unplan="${i}" aria-label="Remove step ${i + 1}">×</button></li>`).join('')
      || '<li class="muted" style="list-style:none;margin-left:-30px">No cables yet. Pick one above or select a cable on the map.</li>';
  }

  async function runPlan() {
    if (!plan.length) { say('Add at least one cable to the plan first.'); SFX.error(); return; }
    try {
      const r = await api('plan', { links: plan.map(c => c.a + ':' + c.b).join(',') });
      const out = $('#plan-result');
      if (r.step === -1) {
        planBreak = null;
        out.innerHTML = `<div class="plan-out">The network stays in one piece through all ${plan.length} steps.</div>`;
        say(`Good plan: removing all ${plan.length} cables in this order never splits the network.`);
        SFX.fix();
      } else if (r.step === 0) {
        planBreak = null;
        out.innerHTML = '<div class="plan-out bad">The network is already split. Repair it first.</div>';
        say('The network is already split before the plan starts.');
        SFX.error();
      } else {
        planBreak = r.step - 1;
        out.innerHTML = `<div class="plan-out bad">Splits at step ${r.step}: unplugging <b>${esc(r.a)} ↔ ${esc(r.b)}</b>. Steps before it are safe.</div>`;
        say(`Binary search says the network splits at step ${r.step}, when ${r.a} ↔ ${r.b} is unplugged.`);
        SFX.fail();
      }
      renderPlan();
    } catch (e) { offline(); }
  }

  $('#plan-add').addEventListener('click', () => {
    const v = $('#plan-select').value;
    if (!v) return;
    const [a, b] = v.split(':');
    addToPlan(a, b);
  });
  $('#plan-run').addEventListener('click', runPlan);
  $('#plan-clear').addEventListener('click', () => { plan = []; planBreak = null; $('#plan-result').innerHTML = ''; renderPlan(); });
  $('#plan-list').addEventListener('click', e => {
    const b = e.target.closest('[data-unplan]');
    if (!b) return;
    plan.splice(+b.dataset.unplan, 1);
    planBreak = null; $('#plan-result').innerHTML = '';
    renderPlan();
  });

  function drawCourier(t) {
    if (!route || route.path.length < 2) return;
    const pts = route.path.map(n => shown[n]);
    const segs = pts.length - 1;
    const p = (t * 0.35) % 1 * segs;
    const i = Math.floor(p), f = p - i;
    const A = pts[i], B = pts[i + 1];
    if (!A || !B) return;
    const x = Math.round(A.x + (B.x - A.x) * f), y = Math.round(A.y + (B.y - A.y) * f);
    ctx.fillStyle = C.k; ctx.fillRect(x - 8, y - 8, 16, 16);
    ctx.fillStyle = C.a; ctx.fillRect(x - 5, y - 5, 10, 10);
    ctx.fillStyle = C.w; ctx.fillRect(x - 3, y - 3, 4, 4);
  }

  function drawDevices(t) {
    const sw = IW * scale, sh = IH * scale;
    S.devices.forEach(d => {
      const p = shown[d.name];
      if (!p) return;
      const dimmed = reach && !reach.reachable.has(d.name);
      let x = Math.round(p.x - sw / 2), y = Math.round(p.y - sh / 2);
      if (!d.working && !reduceMotion) x += Math.floor(t * 12) % 2; // little shake when down

      // floor shadow
      ctx.fillStyle = 'rgba(42,30,20,0.18)';
      ctx.fillRect(x + scale, y + sh - scale, sw - scale * 2, scale * 2);

      ctx.globalAlpha = dimmed ? 0.35 : 1;
      ctx.drawImage(sprite(d.type, !d.working), x, y, sw, sh);
      ctx.globalAlpha = 1;

      // smoke from failed devices
      if (!d.working) {
        for (let k = 0; k < 3; k++) {
          const ph = reduceMotion ? 0.3 + k * 0.2 : (t * 0.6 + k / 3) % 1;
          const s = Math.round(4 + ph * 6);
          ctx.fillStyle = `rgba(110, 90, 68, ${0.7 * (1 - ph)})`;
          ctx.fillRect(Math.round(p.x - 6 + k * 6 + Math.sin(ph * 6 + k) * 4), Math.round(y - ph * 30), s, s);
        }
        // red X
        ctx.fillStyle = C.r;
        for (let i = -3; i <= 3; i++) {
          ctx.fillRect(p.x + sw / 2 - 6 + i * 2, y + 4 + (i + 3) * 2, 3, 3);
          ctx.fillRect(p.x + sw / 2 - 6 + i * 2, y + 16 - (i + 3) * 2, 3, 3);
        }
      }

      // "!" bubble over single points of failure
      if (d.critical && d.working) {
        const bob = reduceMotion ? 0 : Math.round(Math.sin(t * 4 + p.x) * 2);
        const bx = Math.round(p.x + sw / 2 - 4), by = y - 14 + bob;
        ctx.fillStyle = C.k; ctx.fillRect(bx - 2, by - 2, 16, 18);
        ctx.fillStyle = C.a; ctx.fillRect(bx, by, 12, 14);
        ctx.fillStyle = C.k; ctx.fillRect(bx + 5, by + 2, 3, 7); ctx.fillRect(bx + 5, by + 10, 3, 3);
      }

      // working but cut off from the main network: "no signal" bubble
      if (d.working && mainGroup !== null && d.group !== mainGroup) {
        const bx = Math.round(p.x - sw / 2 - 10), by = y - 10;
        ctx.fillStyle = C.k; ctx.fillRect(bx - 2, by - 2, 18, 16);
        ctx.fillStyle = C.l; ctx.fillRect(bx, by, 14, 12);
        ctx.fillStyle = C.r;
        for (let i = 0; i < 4; i++) { ctx.fillRect(bx + 3 + i * 2, by + 2 + i * 2, 2, 2); ctx.fillRect(bx + 9 - i * 2, by + 2 + i * 2, 2, 2); }
      }

      if (dimmed) {
        ctx.fillStyle = C.n;
        ctx.font = '800 22px "Nunito", sans-serif';
        ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
        ctx.fillText('?', p.x, y - 8);
      }
      if (reach && reach.from === d.name) {
        ctx.strokeStyle = C.s; ctx.lineWidth = 3;
        const r = sw / 2 + 8 + (reduceMotion ? 0 : (Math.floor(t * 3) % 3) * 3);
        ctx.strokeRect(p.x - r, p.y - r, r * 2, r * 2);
      }

      // name label
      ctx.font = '800 16px "Nunito", sans-serif';
      const w = ctx.measureText(d.name).width + 8;
      const ly = y + sh + 4;
      ctx.fillStyle = d.working ? C.parchment : '#e9c3b4';
      ctx.fillRect(Math.round(p.x - w / 2), ly, Math.round(w), 22);
      ctx.strokeStyle = C.k; ctx.lineWidth = 2;
      ctx.strokeRect(Math.round(p.x - w / 2), ly, Math.round(w), 22);
      ctx.fillStyle = C.k; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText(d.name, p.x, ly + 12);

      // selection brackets
      const isSel = sel && sel.kind === 'device' && sel.name === d.name;
      const isHov = hover && hover.kind === 'device' && hover.name === d.name;
      if (isSel || isHov) {
        const pad = isSel && !reduceMotion ? 6 + (Math.floor(t * 4) % 2) * 2 : 6;
        bracket(x - pad, y - pad, sw + pad * 2, sh + pad * 2, isSel ? C.o : C.n);
      }
    });
  }

  function bracket(x, y, w, h, col) {
    const L = 10, T = 4;
    ctx.fillStyle = col;
    [[x, y, 1, 1], [x + w, y, -1, 1], [x, y + h, 1, -1], [x + w, y + h, -1, -1]].forEach(([cx, cy, sx, sy]) => {
      ctx.fillRect(sx > 0 ? cx : cx - L, sy > 0 ? cy : cy - T, L, T);
      ctx.fillRect(sx > 0 ? cx : cx - T, sy > 0 ? cy : cy - L, T, L);
    });
  }

  function frame(ms) {
    const t = ms / 1000;
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
    ctx.imageSmoothingEnabled = false;
    drawFloor();
    if (S) {
      for (const n in pos) {
        if (!shown[n] || reduceMotion || (drag && drag.name === n)) { shown[n] = { ...pos[n] }; continue; }
        shown[n].x += (pos[n].x - shown[n].x) * 0.18;
        shown[n].y += (pos[n].y - shown[n].y) * 0.18;
      }
      drawLinks(t);
      drawPlan(t);
      drawCourier(t);
      drawDevices(t);
    }
    requestAnimationFrame(frame);
  }

  // ---------- map mouse ----------
  function hitTest(px, py) {
    if (!S) return null;
    const half = IW * scale / 2 + 4;
    for (const d of S.devices) {
      const p = shown[d.name];
      if (p && Math.abs(px - p.x) < half && Math.abs(py - p.y) < half + 10) return { kind: 'device', name: d.name };
    }
    for (const l of S.links) {
      const A = shown[l.a], B = shown[l.b];
      if (!A || !B) continue;
      if (segDist(px, py, A, B) < 8) return { kind: 'link', a: l.a, b: l.b, l };
    }
    return null;
  }

  function segDist(px, py, A, B) {
    const dx = B.x - A.x, dy = B.y - A.y;
    const len = dx * dx + dy * dy || 1;
    let u = ((px - A.x) * dx + (py - A.y) * dy) / len;
    u = Math.max(0, Math.min(1, u));
    return Math.hypot(px - (A.x + u * dx), py - (A.y + u * dy));
  }

  function mouse(e) {
    const r = cv.getBoundingClientRect();
    return { x: e.clientX - r.left, y: e.clientY - r.top };
  }

  let drag = null;
  cv.addEventListener('pointerdown', e => {
    const m = mouse(e);
    const h = hitTest(m.x, m.y);
    if (h && h.kind === 'device') {
      drag = { name: h.name, sx: m.x, sy: m.y, moved: false, start: { ...grid[h.name] } };
      cv.setPointerCapture(e.pointerId);
    } else {
      select(h);
    }
  });
  cv.addEventListener('pointermove', e => {
    const m = mouse(e);
    if (drag) {
      if (Math.hypot(m.x - drag.sx, m.y - drag.sy) > 6) drag.moved = true;
      if (drag.moved) {
        const g = toGrid(m.x, m.y);
        g.x = Math.max(0, Math.min(30, g.x));
        g.y = Math.max(0, Math.min(20, g.y));
        const taken = Object.keys(grid).some(n => n !== drag.name && grid[n].x === g.x && grid[n].y === g.y);
        if (!taken) { grid[drag.name] = g; pos[drag.name] = { x: ox + g.x * cx, y: oy + g.y * cy }; }
      }
      return;
    }
    hover = hitTest(m.x, m.y);
    cv.style.cursor = hover ? (hover.kind === 'device' ? 'grab' : 'pointer') : 'default';
  });
  cv.addEventListener('pointerup', () => {
    if (!drag) return;
    const d = drag; drag = null;
    if (!d.moved) { select({ kind: 'device', name: d.name }); return; }
    const g = grid[d.name];
    if (g.x !== d.start.x || g.y !== d.start.y) {
      api('move', { name: d.name, x: g.x, y: g.y }).catch(offline);
      layoutMap();
    }
  });
  cv.addEventListener('pointerleave', () => { hover = null; });

  function select(h) {
    sel = h ? (h.kind === 'device' ? { kind: 'device', name: h.name } : { kind: 'link', a: h.a, b: h.b }) : null;
    if (sel) SFX.select();
    reach = null;
    renderSelected();
    if (sel) showTab('control');
  }

  // ---------- actions ----------
  async function toggleSelected() {
    if (!sel) return;
    if (sel.kind === 'device') {
      const d = findDevice(sel.name);
      if (d.working) await act('fail-device', { name: d.name }, SFX.fail);
      else await act('restore-device', { name: d.name }, SFX.fix);
    } else {
      const l = findLink(sel.a, sel.b);
      if (l.working) await act('fail-link', { a: l.a, b: l.b }, SFX.fail);
      else await act('restore-link', { a: l.a, b: l.b }, SFX.fix);
    }
  }

  async function showReach(name) {
    try {
      const r = await api('reach', { from: name });
      reach = { from: name, reachable: new Set(r.reachable) };
      const lost = r.unreachable.filter(n => findDevice(n).working);
      say(lost.length
        ? `From ${name}, BFS reaches ${r.reachable.length} devices. Cut off: ${lost.join(', ')}.`
        : `From ${name}, BFS reaches every working device (${r.reachable.length}).`);
      $('#map-hint').textContent = 'Showing reach from ' + name + '. Click empty floor to clear.';
      SFX.route();
    } catch (e) { offline(); }
  }

  async function findRoute(from, to) {
    if (from === to) { say('Pick two different devices.'); SFX.error(); return; }
    try {
      const r = await api('path', { from, to });
      const box = $('#route-result');
      if (!r.found) {
        route = null;
        box.innerHTML = `<div class="route-card none"><b>No route.</b><p>${esc(from)} can't reach ${esc(to)} with the current failures. Repair something on the way, or add a backup cable.</p></div>`;
        say(`No working path from ${from} to ${to}.`);
        SFX.error();
        return;
      }
      route = { path: r.path, cost: r.cost };
      const steps = p => p.map(esc).map(n => `<span>${n}</span>`).join('→');
      box.innerHTML = `
        <div class="route-card"><b>Best route (Dijkstra)</b>
          <div class="route-steps">${steps(r.path)}</div>
          <div class="route-cost">${r.cost} ms total, ${r.hops} hop${r.hops === 1 ? '' : 's'}</div>
        </div>
        ${r.alternates.length === 1 ? '<p class="warn-line">This is the only route. One failure on it cuts the connection.</p>' : ''}
        ${r.alternates.length > 1 ? '<h3>All routes found (backtracking)</h3>' : ''}
        ${r.alternates.length > 1 ? r.alternates.map((a, i) =>
          `<div class="alt ${a.path.join() === r.path.join() ? 'on' : ''}" data-alt="${i}" tabindex="0">${a.cost} ms: ${a.path.map(esc).join(' → ')}</div>`).join('') : ''}`;
      box.querySelectorAll('.alt').forEach(el => {
        const pick = () => {
          const a = r.alternates[+el.dataset.alt];
          route = { path: a.path, cost: a.cost };
          box.querySelectorAll('.alt').forEach(x => x.classList.toggle('on', x === el));
          SFX.select();
        };
        el.addEventListener('click', pick);
        el.addEventListener('keydown', e => { if (e.key === 'Enter') pick(); });
      });
      say(`Fastest way from ${from} to ${to}: ${r.cost} ms over ${r.hops} hops. ${r.alternates.length > 1 ? (r.alternates.length) + ' routes exist in total.' : 'There is no backup route.'}`);
      SFX.route();
    } catch (e) { offline(); }
  }

  // ---------- untangle mode ----------
  // Layered layout: BFS depth from the top device decides the row, then a few
  // "barycenter" sweeps order each row by the average position of its neighbours,
  // which removes most crossing cables.
  function crossings() {
    let n = 0;
    const L = S.links;
    for (let i = 0; i < L.length; i++)
      for (let j = i + 1; j < L.length; j++) {
        const a = L[i], b = L[j];
        if (a.a === b.a || a.a === b.b || a.b === b.a || a.b === b.b) continue;
        if (segCross(grid[a.a], grid[a.b], grid[b.a], grid[b.b])) n++;
      }
    return n;
  }
  function segCross(p1, p2, p3, p4) {
    const d = (a, b, c) => (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    const d1 = d(p3, p4, p1), d2 = d(p3, p4, p2), d3 = d(p1, p2, p3), d4 = d(p1, p2, p4);
    return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
  }

  function untangle(announce) {
    if (!S || !S.devices.length) return;
    const before = crossings();
    const names = S.devices.map(d => d.name);
    const nb = {};
    names.forEach(n => (nb[n] = []));
    S.links.forEach(l => { nb[l.a].push(l.b); nb[l.b].push(l.a); });

    // root = the device highest on the map (ISP / Internet / Router1)
    const root = names.slice().sort((a, b) => grid[a].y - grid[b].y || nb[b].length - nb[a].length)[0];
    const depth = { [root]: 0 };
    const queue = [root];
    while (queue.length) {
      const u = queue.shift();
      nb[u].forEach(v => { if (depth[v] === undefined) { depth[v] = depth[u] + 1; queue.push(v); } });
    }
    const maxD = Math.max(...Object.values(depth));
    names.forEach(n => { if (depth[n] === undefined) depth[n] = maxD + 1; }); // not connected at all

    const rows = [];
    names.forEach(n => (rows[depth[n]] = rows[depth[n]] || []).push(n));
    rows.forEach(r => r.sort((a, b) => grid[a].x - grid[b].x));

    const place = {};
    const setPlace = () => rows.forEach(r => r.forEach((n, i) => (place[n] = (i + 0.5) / r.length)));
    setPlace();
    for (let it = 0; it < 12; it++) {
      const down = it % 2 === 0;
      const order = down ? rows.map((_, i) => i) : rows.map((_, i) => rows.length - 1 - i);
      order.forEach(ri => {
        const want = ri + (down ? -1 : 1);
        const bary = {};
        rows[ri].forEach(n => {
          const ns = nb[n].filter(v => depth[v] === want);
          bary[n] = ns.length ? ns.reduce((s, v) => s + place[v], 0) / ns.length : place[n];
        });
        rows[ri].sort((a, b) => bary[a] - bary[b]);
        rows[ri].forEach((n, i) => (place[n] = (i + 0.5) / rows[ri].length));
      });
    }

    const widest = Math.max(...rows.map(r => (r ? r.length : 0)));
    const old = {};
    names.forEach(n => (old[n] = { ...grid[n] }));
    rows.forEach((r, y) => {
      if (!r) return;
      r.forEach((n, i) => (grid[n] = { x: Math.round((i + 0.5) * widest / r.length - 0.5), y: y }));
    });
    // keep whichever layout has fewer crossings
    const after = crossings();
    if (after > before) {
      names.forEach(n => (grid[n] = old[n]));
      if (announce) say(`This layout already has the fewest crossings I can find (${before}).`);
      layoutMap();
      return;
    }
    names.forEach(n => {
      if (old[n].x !== grid[n].x || old[n].y !== grid[n].y)
        api('move', { name: n, x: grid[n].x, y: grid[n].y }).catch(() => {});
    });
    layoutMap();
    if (announce) say(before === after
      ? `No cables cross. Rows now follow the distance from ${root}. Press Save to keep this layout.`
      : `Untangled: crossing cables went from ${before} to ${after}. Press Save to keep this layout.`);
  }

  $('#untangle-btn').addEventListener('click', e => {
    untangleOn = !untangleOn;
    e.currentTarget.setAttribute('aria-pressed', untangleOn);
    e.currentTarget.textContent = untangleOn ? 'Untangle: on' : 'Untangle';
    if (untangleOn) { untangle(true); SFX.route(); }
    else { say('Untangle mode off. Drag devices to place them yourself.'); SFX.select(); }
  });

  // ---------- tabs ----------
  function showTab(name) {
    $$('.tab').forEach(t => {
      const on = t.dataset.tab === name;
      t.classList.toggle('active', on);
      t.setAttribute('aria-selected', on);
    });
    $$('.pane').forEach(p => p.classList.toggle('hidden', p.id !== 'pane-' + name));
  }
  $$('.tab').forEach(t => t.addEventListener('click', () => showTab(t.dataset.tab)));

  // ---------- wiring ----------
  $('#selected-body').addEventListener('click', e => {
    const b = e.target.closest('[data-do]');
    if (!b) return;
    const what = b.dataset.do;
    if (what === 'fail' || what === 'fix' || what === 'cut' || what === 'splice') toggleSelected();
    else if (what === 'reach') showReach(sel.name);
    else if (what === 'plan') { addToPlan(sel.a, sel.b); showTab('intel'); }
    else if (what === 'route-from') {
      $('#route-form [name=from]').value = sel.name;
      showTab('route');
      say('Now choose where to go and press Find route.');
    }
  });

  document.addEventListener('click', e => {
    const p = e.target.closest('[data-pick]');
    if (p) { select({ kind: 'device', name: p.dataset.pick }); return; }
    const l = e.target.closest('[data-link]');
    if (l) { const [a, b] = l.dataset.link.split('|'); select({ kind: 'link', a, b }); }
  });

  cv.addEventListener('click', e => {
    const m = mouse(e);
    if (!hitTest(m.x, m.y) && reach) { reach = null; $('#map-hint').textContent = ''; }
  });

  $('#undo-btn').addEventListener('click', () => act('undo', null, SFX.fix));
  $('#restore-all-btn').addEventListener('click', () => act('restore-all', null, SFX.fix));
  $('#save-btn').addEventListener('click', async () => {
    await act('save');
    if (window.NFA_READY) say('Online demo: the network is saved only until you close this tab. Run server.exe locally to save to the data folder.');
  });
  $('#load-btn').addEventListener('click', () => act('load', { name: $('#net-select').value }, SFX.route));

  $('#add-device-form').addEventListener('submit', e => {
    e.preventDefault();
    const f = new FormData(e.target);
    const used = new Set(Object.values(grid).map(g => g.x + ',' + g.y));
    const spot = freeCell(used);
    act('add-device', { name: f.get('name').trim(), type: f.get('type'), x: spot.x, y: spot.y }).then(d => {
      if (d && d.ok) { e.target.reset(); say(`${f.get('name')} placed on the map. Drag it where you like, then add a cable.`); }
    });
  });

  $('#add-link-form').addEventListener('submit', e => {
    e.preventDefault();
    const f = new FormData(e.target);
    act('add-link', { a: f.get('a'), b: f.get('b'), latency: f.get('latency') });
  });

  $('#route-form').addEventListener('submit', e => {
    e.preventDefault();
    const f = new FormData(e.target);
    findRoute(f.get('from'), f.get('to'));
  });
  $('#clear-route').addEventListener('click', () => { route = null; $('#route-result').innerHTML = ''; });

  document.addEventListener('keydown', e => {
    if (e.target.matches('input, select, textarea')) return;
    if (e.key === 'f' || e.key === 'F') toggleSelected();
    else if (e.key === 'u' || e.key === 'U') act('undo', null, SFX.fix);
    else if (e.key === 'Escape') { sel = null; reach = null; route = null; $('#map-hint').textContent = ''; renderSelected(); }
  });

  $('#sound-btn').addEventListener('click', e => {
    soundOn = !soundOn;
    e.currentTarget.setAttribute('aria-pressed', soundOn);
  });

  window.addEventListener('resize', layoutMap);

  // ---------- start ----------
  async function boot() {
    drawPixels($('#bot'), BOT, 4);
    // title art: a little row of devices
    const art = $('#start-art').getContext('2d');
    ['router', 'switch', 'server', 'pc'].forEach((t, i) => art.drawImage(sprite(t, false), 4 + i * 42, 18, 36, 30));
    art.fillStyle = C.s;
    for (let i = 0; i < 3; i++) art.fillRect(40 + i * 42, 33, 6, 3);

    layoutMap();
    requestAnimationFrame(frame);
    if (document.fonts) document.fonts.ready.then(layoutMap);
    try {
      const nets = await api('networks');
      $('#net-select').innerHTML = nets.list.map(n => `<option value="${esc(n)}">${esc(n)}</option>`).join('');
      $('#net-select').value = nets.current;
      setState(await api('state'));
      say(`Welcome. This is ${S.name}. Click a device and fail it to see what breaks. Orange "!" marks a single point of failure.`);
    } catch (e) {
      offline();
    }
  }

  $('#start-btn').addEventListener('click', () => {
    $('#start').classList.add('hidden');
    beep([[523, 0.06], [659, 0.06], [784, 0.06], [1047, 0.12]]);
    layoutMap();
  });

  boot();
})();
