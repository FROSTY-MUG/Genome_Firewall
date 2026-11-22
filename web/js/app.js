import { getHealth } from './api.js';
import { icon, toast, download, copyText, esc, fmtInt } from './ui.js';
import { renderOverview } from './views/overview.js';
import { renderScanner } from './views/scanner.js';
import { renderReport, buildCsv, buildSummary } from './views/report.js';

const LS_SETTINGS = 'gf.settings.v2';
const LS_REPORT = 'gf.report.v2';
const ROUTES = ['/', '/scan', '/report'];

const DEFAULT_SETTINGS = {
  db: 'markers_card_top.csv',
  fuzzy: true,
  async: true,
  confidence: 75,
};

function readStorage(key, fallback) {
  try {
    const raw = localStorage.getItem(key);
    return raw ? JSON.parse(raw) : fallback;
  } catch (_) {
    return fallback;
  }
}

function writeStorage(key, value) {
  try { localStorage.setItem(key, JSON.stringify(value)); } catch (_) { /* quota / private mode */ }
}

const state = {
  route: '/',
  health: null,
  healthError: null,
  settings: { ...DEFAULT_SETTINGS, ...readStorage(LS_SETTINGS, {}) },
  lastResult: readStorage(LS_REPORT, null),
  pendingSample: null,
};

const main = document.getElementById('main');
const chip = document.getElementById('engine-chip');
const chipDot = document.getElementById('engine-dot');
const chipText = document.getElementById('engine-text');
const palette = document.getElementById('palette');
const paletteInput = document.getElementById('palette-input');
const paletteList = document.getElementById('palette-list');
const paletteCount = document.getElementById('palette-count');

let cleanup = null;

// ------------------------------------------------------------------ routing

function currentRoute() {
  const hash = window.location.hash.replace(/^#/, '');
  return ROUTES.includes(hash) ? hash : '/';
}

function navigate(path) {
  const target = ROUTES.includes(path) ? path : '/';
  if (currentRoute() === target && window.location.hash) {
    renderRoute();
    return;
  }
  window.location.hash = target;
}

function renderRoute() {
  const route = currentRoute();
  const changed = route !== state.route;
  state.route = route;

  if (cleanup) {
    try { cleanup(); } catch (_) { /* view already torn down */ }
    cleanup = null;
  }

  main.innerHTML = '';
  const ctx = {
    navigate,
    saveSettings(settings) {
      state.settings = { ...settings };
      writeStorage(LS_SETTINGS, state.settings);
    },
    setLastResult(result) {
      state.lastResult = result;
      writeStorage(LS_REPORT, result);
    },
    consumePendingSample() {
      const sample = state.pendingSample;
      state.pendingSample = null;
      return sample;
    },
  };

  let view;
  if (route === '/scan') {
    view = renderScanner(state, ctx);
    document.title = 'New analysis — Genome Firewall';
  } else if (route === '/report') {
    view = renderReport(state, ctx);
    document.title = 'Decision report — Genome Firewall';
  } else {
    view = renderOverview(state);
    document.title = 'Genome Firewall — AMR biosecurity console';
  }

  main.appendChild(view);
  cleanup = typeof view.__cleanup === 'function' ? view.__cleanup : null;

  document.querySelectorAll('[data-route-link]').forEach((link) => {
    const active = link.getAttribute('data-route-link') === route;
    link.classList.toggle('is-active', active);
    if (active) link.setAttribute('aria-current', 'page');
    else link.removeAttribute('aria-current');
  });

  if (changed) window.scrollTo({ top: 0, behavior: 'auto' });
}

// ------------------------------------------------------------------- header

function renderEngineChip() {
  chipDot.className = 'dot';
  if (state.health && state.health.status === 'ok') {
    chipDot.classList.add('dot--ok');
    const cores = state.health.host?.cpus || '—';
    chipText.textContent = `engine ready · ${cores} cores`;
    chip.title = `Engine ${state.health.engine.version} · ${state.health.host.platform}/${state.health.host.arch} · ${cores} CPUs · Node ${state.health.host.node}`;
  } else if (state.health && state.health.status === 'engine-missing') {
    chipDot.classList.add('dot--warn');
    chipText.textContent = 'binary missing';
    chip.title = 'Compile the engine with `make` in the repository root, then restart the API server.';
  } else {
    chipDot.classList.add('dot--err');
    chipText.textContent = 'engine offline';
    chip.title = 'The local API is unreachable. Start it with `node api/server.js` (or `npm start` in api/).';
  }
}

async function refreshHealth({ announce = false } = {}) {
  try {
    state.health = await getHealth();
    state.healthError = null;
    renderEngineChip();
    if (announce) {
      const dbs = state.health.databases?.map((d) => `${d.id} (${fmtInt(d.markers)})`).join(', ');
      toast(dbs || 'Engine reachable', { title: `Engine ${state.health.engine.version} ready` });
    }
  } catch (err) {
    state.healthError = err;
    renderEngineChip();
    if (announce) toast(err.message || 'API unreachable', { tone: 'err', title: 'Engine offline' });
  }
  return state.health;
}

chip?.addEventListener('click', () => refreshHealth({ announce: true }));

// ------------------------------------------------------------------ palette

function paletteActions() {
  const list = [
    { group: 'Navigate', label: 'Overview', hint: 'home', icon: 'shield', run: () => navigate('/') },
    { group: 'Navigate', label: 'New analysis', hint: 'scanner', icon: 'scan', run: () => navigate('/scan') },
    { group: 'Navigate', label: 'Open last report', hint: state.lastResult ? 'saved' : 'empty', icon: 'file', run: () => navigate('/report') },
    { group: 'Run', label: 'Load MDR demo genome', hint: 'recommended', icon: 'flask', run: () => { state.pendingSample = 'sample_real.fasta'; navigate('/scan'); } },
    { group: 'Run', label: 'Load E. coli K12 baseline', hint: 'demo', icon: 'dna', run: () => { state.pendingSample = 'sample.fasta'; navigate('/scan'); } },
    { group: 'Run', label: 'Load stress genome', hint: 'performance', icon: 'zap', run: () => { state.pendingSample = 'sample_large.fasta'; navigate('/scan'); } },
    { group: 'Report', label: 'Download JSON', hint: 'export', icon: 'download', run: () => exportReport('json') },
    { group: 'Report', label: 'Download CSV', hint: 'export', icon: 'download', run: () => exportReport('csv') },
    { group: 'Report', label: 'Copy summary', hint: 'clipboard', icon: 'copy', run: () => copySummary() },
    { group: 'System', label: 'Refresh engine status', hint: 'health', icon: 'refresh', run: () => refreshHealth({ announce: true }) },
    { group: 'System', label: 'Open API health JSON', hint: 'debug', icon: 'external', run: () => window.open('/api/health', '_blank', 'noopener') },
  ];
  return list;
}

function exportReport(kind) {
  if (!state.lastResult) {
    toast('Run a scan first — no report is stored', { tone: 'err' });
    return;
  }
  const base = String(state.lastResult.input?.fileName || 'genome').replace(/\.[^.]+$/, '');
  if (kind === 'json') {
    download(`genome-firewall-${base}.json`, JSON.stringify(state.lastResult, null, 2), 'application/json');
    toast('JSON report downloaded');
  } else {
    download(`genome-firewall-${base}.csv`, buildCsv(state.lastResult), 'text/csv');
    toast('CSV report downloaded');
  }
}

async function copySummary() {
  if (!state.lastResult) {
    toast('Run a scan first — no report is stored', { tone: 'err' });
    return;
  }
  const ok = await copyText(buildSummary(state.lastResult));
  toast(ok ? 'Report summary copied to clipboard' : 'Could not access the clipboard', { tone: ok ? 'ok' : 'err' });
}

let paletteItems = [];
let paletteIndex = 0;

function renderPalette(query = '') {
  const q = query.trim().toLowerCase();
  paletteItems = paletteActions().filter((a) => !q || `${a.label} ${a.hint} ${a.group}`.toLowerCase().includes(q));
  paletteIndex = Math.min(paletteIndex, Math.max(0, paletteItems.length - 1));

  if (!paletteItems.length) {
    paletteList.innerHTML = `<div class="palette__none">No actions match “${esc(query)}”</div>`;
    paletteCount.textContent = '0 actions';
    return;
  }

  let lastGroup = '';
  paletteList.innerHTML = paletteItems.map((action, index) => {
    const group = action.group !== lastGroup
      ? `<div class="palette__group">${esc(action.group)}</div>`
      : '';
    lastGroup = action.group;
    return `${group}
      <button class="palette__item ${index === paletteIndex ? 'is-active' : ''}" type="button" role="option" aria-selected="${index === paletteIndex}" data-index="${index}">
        ${icon(action.icon)}
        <span>${esc(action.label)}</span>
        ${action.hint ? `<kbd>${esc(action.hint)}</kbd>` : ''}
      </button>`;
  }).join('');

  paletteCount.textContent = `${paletteItems.length} action${paletteItems.length === 1 ? '' : 's'}`;
  paletteList.querySelectorAll('[data-index]').forEach((btn) => {
    btn.addEventListener('click', () => runPaletteItem(Number(btn.dataset.index)));
    btn.addEventListener('mousemove', () => {
      const index = Number(btn.dataset.index);
      if (index === paletteIndex) return;
      paletteIndex = index;
      paletteList.querySelectorAll('.palette__item').forEach((el, i) => {
        el.classList.toggle('is-active', i === index);
        el.setAttribute('aria-selected', String(i === index));
      });
    });
  });
  paletteList.querySelector('.is-active')?.scrollIntoView({ block: 'nearest' });
}

function runPaletteItem(index) {
  const action = paletteItems[index];
  if (!action) return;
  closePalette();
  setTimeout(() => action.run(), 10);
}

function openPalette() {
  if (!palette) return;
  paletteIndex = 0;
  renderPalette('');
  paletteInput.value = '';
  if (typeof palette.showModal === 'function') {
    if (!palette.open) palette.showModal();
  } else {
    palette.setAttribute('open', '');
  }
  setTimeout(() => paletteInput?.focus(), 20);
}

function closePalette() {
  if (!palette) return;
  if (typeof palette.close === 'function' && palette.open) palette.close();
  else palette.removeAttribute('open');
}

document.getElementById('palette-open')?.addEventListener('click', openPalette);
palette?.addEventListener('click', (e) => {
  if (e.target === palette) closePalette();
});
palette?.addEventListener('cancel', (e) => { e.preventDefault(); closePalette(); });

paletteInput?.addEventListener('input', () => {
  paletteIndex = 0;
  renderPalette(paletteInput.value);
});

paletteInput?.addEventListener('keydown', (e) => {
  if (e.key === 'ArrowDown') {
    e.preventDefault();
    paletteIndex = Math.min(paletteItems.length - 1, paletteIndex + 1);
    renderPalette(paletteInput.value);
  } else if (e.key === 'ArrowUp') {
    e.preventDefault();
    paletteIndex = Math.max(0, paletteIndex - 1);
    renderPalette(paletteInput.value);
  } else if (e.key === 'Enter') {
    e.preventDefault();
    runPaletteItem(paletteIndex);
  } else if (e.key === 'Escape') {
    closePalette();
  }
});

// ------------------------------------------------------------- global events

document.addEventListener('keydown', (e) => {
  const isTyping = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement?.tagName || '');
  if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 'k') {
    e.preventDefault();
    if (palette?.open) closePalette(); else openPalette();
    return;
  }
  if (e.key === 'Escape' && palette?.open) {
    closePalette();
    return;
  }
  if (e.key === 'Enter' && !isTyping && state.route === '/scan') {
    const runBtn = document.getElementById('run-btn');
    if (runBtn && !runBtn.disabled) runBtn.click();
  }
});

document.addEventListener('click', (e) => {
  const navTarget = e.target.closest('[data-nav], [data-route-link]');
  if (navTarget) {
    const path = navTarget.getAttribute('data-nav') || navTarget.getAttribute('data-route-link');
    if (path) {
      e.preventDefault();
      navigate(path);
      return;
    }
  }
  const demo = e.target.closest('[data-action="demo"]');
  if (demo) {
    e.preventDefault();
    state.pendingSample = demo.getAttribute('data-sample') || 'sample_real.fasta';
    navigate('/scan');
  }
});

window.addEventListener('hashchange', renderRoute);

window.addEventListener('unhandledrejection', (event) => {
  const message = event.reason?.message || 'Unexpected error';
  if (/Failed to fetch|NetworkError/i.test(message)) {
    toast('The local API is unreachable. Start it with `node api/server.js`.', { tone: 'err', title: 'Engine offline' });
  }
});

// --------------------------------------------------------------------- boot

async function boot() {
  renderEngineChip();
  try {
    await Promise.race([
      refreshHealth(),
      new Promise((resolve) => setTimeout(resolve, 3500)),
    ]);
  } catch (_) { /* rendered as offline */ }

  renderRoute();

  const bootEl = document.getElementById('boot');
  if (bootEl) {
    bootEl.classList.add('is-hidden');
    setTimeout(() => bootEl.remove(), 500);
  }
}

boot();