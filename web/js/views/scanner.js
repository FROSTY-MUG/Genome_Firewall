import { icon, esc, fmtBytes, fmtInt, fmtMs, parseFastaStats, toast } from '../ui.js';
import { getSample, streamScan } from '../api.js';

const STAGES = [
  { id: 'validate', name: 'Validate input' },
  { id: 'index', name: 'Index catalogue' },
  { id: 'scan', name: 'Scan genome' },
  { id: 'infer', name: 'Infer probabilities' },
  { id: 'report', name: 'Compile report' },
];

const FALLBACK_DBS = [
  { id: 'markers_card_top.csv', label: 'CARD Top Markers', description: '455 curated CARD homologues across 11 drug classes.', markers: 455, classes: new Array(11), recommended: true },
  { id: 'markers_large.csv', label: 'Extended Panel', description: '53 multi-marker signatures from NCBI, CARD and ResFinder.', markers: 53, classes: new Array(10) },
  { id: 'markers.csv', label: 'Core Panel', description: '10 hand-curated markers, fastest to index.', markers: 10, classes: new Array(5) },
];

const FALLBACK_SAMPLES = [
  { id: 'sample_real.fasta', label: 'MDR E. coli', badge: 'Demo', available: true },
  { id: 'sample_large.fasta', label: 'Stress genome', badge: 'Perf', available: true },
  { id: 'sample.fasta', label: 'E. coli K12', badge: 'Baseline', available: true },
];

export function renderScanner(state, ctx) {
  const databases = (state.health?.databases?.length ? state.health.databases : FALLBACK_DBS)
    .filter((d) => d.available !== false);
  const samples = (state.health?.samples?.length ? state.health.samples : FALLBACK_SAMPLES)
    .filter((s) => s.available !== false);

  const settings = { ...state.settings };
  let input = null;
  let mode = 'upload';
  let running = false;
  let controller = null;
  let timer = null;
  let startedAt = 0;
  let logs = [];
  let stageState = {};
  let lastOutcome = null;

  const root = document.createElement('div');
  root.innerHTML = `
    <div class="container">
      <div class="page-head">
        <div class="page-head__text">
          <a class="back-link" href="#/" data-nav="/">${icon('chevron-left')} Overview</a>
          <h1 class="page-head__title">New analysis</h1>
          <p class="lead">Ingest a bacterial genome and score it against a resistance catalogue.</p>
        </div>
        <div class="page-head__actions">
          ${state.lastResult ? `<a class="btn" href="#/report" data-nav="/report">${icon('file')} Last report</a>` : ''}
        </div>
      </div>

      <div class="workbench">
        <div class="workbench__main">
          <section class="panel step-card" aria-labelledby="input-title">
            <div class="step-card__head">
              <div class="step-card__title">
                <div class="step-card__id">01 / GENOME</div>
                <h2 id="input-title">Sequence input</h2>
                <p>FASTA, FA, FAS — parsed locally before it is sent to the engine.</p>
              </div>
              <div class="segmented" role="tablist" aria-label="Input method">
                <button type="button" role="tab" aria-selected="true" data-mode="upload">Upload</button>
                <button type="button" role="tab" aria-selected="false" data-mode="paste">Paste</button>
              </div>
            </div>

            <div id="input-body"></div>

            <div class="sample-row">
              <span class="sample-row__label">Demo genomes</span>
              ${samples.map((s) => `
                <button class="chip" type="button" data-sample="${esc(s.id)}" title="${esc(s.description || '')}">
                  ${icon('dna')} ${esc(s.label)}${s.badge ? ` <span class="faint">· ${esc(s.badge)}</span>` : ''}
                </button>`).join('')}
            </div>
          </section>

          <section class="panel step-card" aria-labelledby="config-title">
            <div class="step-card__head">
              <div class="step-card__title">
                <div class="step-card__id">02 / PARAMETERS</div>
                <h2 id="config-title">Scan configuration</h2>
                <p>Reference catalogue, mutation tolerance and the confidence gate.</p>
              </div>
            </div>

            <div class="db-options" role="radiogroup" aria-label="Reference database">
              ${databases.map((db) => `
                <label class="db-option">
                  <input type="radio" name="db" value="${esc(db.id)}" ${db.id === settings.db ? 'checked' : ''} />
                  <span class="db-option__radio" aria-hidden="true"></span>
                  <div>
                    <div class="db-option__name">${esc(db.label)} ${db.recommended ? '<span class="badge-rec">recommended</span>' : ''}</div>
                    <div class="db-option__desc">${esc(db.description)}</div>
                  </div>
                  <div class="db-option__count"><b>${fmtInt(db.markers)}</b>markers</div>
                </label>`).join('')}
            </div>

            <div class="toggle-grid">
              <label class="toggle">
                <input type="checkbox" id="opt-fuzzy" ${settings.fuzzy ? 'checked' : ''} />
                <span class="switch" aria-hidden="true"></span>
                <span class="toggle__text"><b>SNP tolerance</b><span>Allow Hamming distance ≤ 1</span></span>
              </label>
              <label class="toggle">
                <input type="checkbox" id="opt-async" ${settings.async !== false ? 'checked' : ''} />
                <span class="switch" aria-hidden="true"></span>
                <span class="toggle__text"><b>Multithreaded</b><span>Use all available cores</span></span>
              </label>
            </div>

            <div class="slider-row">
              <div class="slider-row__head">
                <span class="slider-row__label">Confidence gate</span>
                <span class="slider-row__value" id="conf-value">${settings.confidence}%</span>
              </div>
              <input type="range" id="conf-range" min="50" max="99" step="1" value="${settings.confidence}" aria-label="Confidence threshold" />
              <div class="slider-row__scale"><span>50% · more calls</span><span>99% · stricter</span></div>
              <p class="slider-row__hint">Calls below this confidence are downgraded to no-call instead of being asserted.</p>
            </div>
          </section>

          <div class="runbar">
            <div class="runbar__summary">
              <b id="run-summary-title">No genome loaded</b>
              <span id="run-summary-meta">Drop a FASTA or pick a demo to begin</span>
            </div>
            <button class="btn btn--danger" type="button" id="cancel-btn" hidden>${icon('x')} Cancel</button>
            <button class="btn btn--primary" type="button" id="run-btn" disabled>${icon('scan')} Run scan</button>
          </div>
        </div>

        <aside class="workbench__aside">
          <section class="panel run-panel" aria-label="Scan telemetry">
            <div class="run-panel__head">
              <span class="run-panel__title"><span class="dot" id="run-dot"></span> Run telemetry</span>
              <span class="run-panel__timer mono" id="run-timer">00:00.0</span>
            </div>
            <div class="run-panel__body">
              <div class="stages" id="stages"></div>
              <div class="console">
                <div class="console__head"><span class="dot"></span> engine stdio</div>
                <div class="console__out" id="console" role="log" aria-live="off"><div class="console__empty">Waiting for a run…</div></div>
              </div>
              <div id="run-feedback"></div>
              <div id="run-result" hidden></div>
            </div>
          </section>

          <div class="panel aside-card">
            <div class="aside-card__head">
              <span class="aside-card__title">${icon('info')} Good to know</span>
            </div>
            <div class="legend">
              <div class="legend__item"><span class="legend__bar legend__bar--1"></span><div><b>Catalogue is loaded per scan</b><span>Indexing 455 markers takes a few hundred milliseconds; the report includes the exact figure.</span></div></div>
              <div class="legend__item"><span class="legend__bar legend__bar--3"></span><div><b>Nothing is uploaded</b><span>The engine runs as a local child process. Sequences are written to a temporary file and deleted immediately.</span></div></div>
            </div>
          </div>
        </aside>
      </div>
    </div>
  `;

  const $ = (sel) => root.querySelector(sel);
  const inputBody = $('#input-body');
  const runBtn = $('#run-btn');
  const cancelBtn = $('#cancel-btn');
  const runDot = $('#run-dot');
  const runTimer = $('#run-timer');
  const consoleEl = $('#console');
  const feedback = $('#run-feedback');
  const resultBox = $('#run-result');
  const stagesEl = $('#stages');
  const summaryTitle = $('#run-summary-title');
  const summaryMeta = $('#run-summary-meta');

  // ---------------------------------------------------------------- stages

  function renderStages() {
    stagesEl.innerHTML = STAGES.map((stage) => {
      const st = stageState[stage.id] || {};
      return `
        <div class="stage ${st.status ? `is-${st.status}` : ''}" data-stage="${stage.id}">
          <span class="stage__bullet">${st.status === 'done' ? icon('check') : st.status === 'error' ? icon('x') : ''}</span>
          <span class="stage__name">${stage.name}</span>
          <span class="stage__detail">${st.detail ? esc(st.detail) : ''}</span>
        </div>`;
    }).join('');
  }

  function setStage(id, status, detail) {
    stageState[id] = { status, detail: detail ?? stageState[id]?.detail };
    renderStages();
  }

  function resetStages() {
    stageState = {};
    STAGES.forEach((s) => { stageState[s.id] = { status: '' }; });
    renderStages();
  }

  // ---------------------------------------------------------------- console

  function appendLog(line) {
    const text = String(line || '').trim();
    if (!text) return;
    logs.push(text);
    if (logs.length > 400) logs = logs.slice(-400);
    const cls = /\[\+\]/.test(text) ? 'is-ok' : /\[!\]|error|fail/i.test(text) ? 'is-err' : /Warn/i.test(text) ? 'is-warn' : '';
    if (consoleEl.querySelector('.console__empty')) consoleEl.innerHTML = '';
    const lineEl = document.createElement('div');
    lineEl.className = `console__line ${cls}`;
    lineEl.textContent = text;
    consoleEl.appendChild(lineEl);
    while (consoleEl.children.length > 400) consoleEl.firstElementChild.remove();
    consoleEl.scrollTop = consoleEl.scrollHeight;
  }

  function clearConsole(message = 'Waiting for a run…') {
    consoleEl.innerHTML = `<div class="console__empty">${esc(message)}</div>`;
  }

  // ------------------------------------------------------------------ input

  function renderInput() {
    if (!input) {
      inputBody.innerHTML = mode === 'upload'
        ? `
          <div class="dropzone" id="dropzone" role="button" tabindex="0" aria-label="Upload a FASTA file">
            <span class="dropzone__icon">${icon('upload')}</span>
            <strong>Drop a FASTA file, or <em>browse</em></strong>
            <span>Up to 40 MB · .fasta, .fa, .fas, .fna</span>
            <input type="file" id="file-input" accept=".fasta,.fa,.fas,.fna,.txt,text/plain" />
          </div>`
        : `
          <div class="paste-wrap">
            <textarea id="paste-area" spellcheck="false" placeholder=">contig_1&#10;ACGT… paste raw FASTA or a naked nucleotide sequence"></textarea>
            <div style="display:flex;justify-content:space-between;gap:10px;align-items:center;flex-wrap:wrap">
              <span class="faint" style="font-size:11px">Minimum 21 unambiguous bases. Headers are optional.</span>
              <button class="btn btn--sm" type="button" id="use-paste">${icon('check')} Use sequence</button>
            </div>
          </div>`;
      bindInput();
      return;
    }

    const firstSeq = input.content.split('\n').find((l) => l && !l.startsWith('>')) || '';
    inputBody.innerHTML = `
      <div class="file-card">
        <span class="file-card__icon">${icon('file')}</span>
        <div style="min-width:0">
          <div class="file-card__name" title="${esc(input.name)}">${esc(input.name)}</div>
          <div class="file-card__meta">
            ${fmtBytes(input.stats.bytes)} · <b>${fmtInt(input.stats.bases)}</b> bases ·
            GC <b>${input.stats.gcPercent.toFixed(1)}%</b> ·
            ${input.stats.headers} header${input.stats.headers === 1 ? '' : 's'}
            ${input.stats.ambiguous ? ` · ${fmtInt(input.stats.ambiguous)} N` : ''}
          </div>
        </div>
        <button class="icon-btn" type="button" id="clear-input" aria-label="Remove genome">${icon('x')}</button>
      </div>
      <div class="preview"><b>›</b> ${esc(firstSeq.slice(0, 120) || 'sequence unavailable')}${firstSeq.length > 120 ? '…' : ''}</div>
    `;
    $('#clear-input').addEventListener('click', () => {
      input = null;
      renderInput();
      refreshRunbar();
    });
  }

  function bindInput() {
    if (mode === 'upload') {
      const zone = $('#dropzone');
      const fileInput = $('#file-input');
      if (!zone) return;
      const open = () => fileInput?.click();
      zone.addEventListener('click', open);
      zone.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); open(); }
      });
      zone.addEventListener('dragover', (e) => { e.preventDefault(); zone.classList.add('is-over'); });
      zone.addEventListener('dragleave', () => zone.classList.remove('is-over'));
      zone.addEventListener('drop', (e) => {
        e.preventDefault();
        zone.classList.remove('is-over');
        const file = e.dataTransfer?.files?.[0];
        if (file) loadFile(file);
      });
      fileInput?.addEventListener('change', () => {
        const file = fileInput.files?.[0];
        if (file) loadFile(file);
      });
    } else {
      const btn = $('#use-paste');
      btn?.addEventListener('click', () => {
        const value = $('#paste-area')?.value || '';
        if (!value.trim()) { toast('Paste a sequence first', { tone: 'err' }); return; }
        acceptContent(value, 'pasted-sequence.fasta');
      });
    }
  }

  async function loadFile(file) {
    if (file.size > 40 * 1024 * 1024) {
      toast('That file is over the 40 MB limit', { tone: 'err' });
      return;
    }
    try {
      const text = await file.text();
      acceptContent(text, file.name);
    } catch (_) {
      toast('Could not read that file', { tone: 'err' });
    }
  }

  function acceptContent(content, name) {
    const stats = parseFastaStats(content);
    if (!stats.valid) {
      toast('No usable sequence found (need 21+ A/C/G/T bases)', { tone: 'err' });
      return;
    }
    input = { content, name, stats };
    renderInput();
    refreshRunbar();
    setStage('validate', 'done', `${fmtInt(stats.bases)} bases`);
    toast(`Loaded ${name}`, { title: 'Genome ready' });
  }

  async function loadSample(id) {
    if (running) return;
    const buttons = [...root.querySelectorAll('[data-sample]')];
    buttons.forEach((b) => { b.disabled = true; });
    try {
      const text = await getSample(id);
      const sample = samples.find((s) => s.id === id);
      acceptContent(text, sample?.id || id);
    } catch (err) {
      toast(err.message || 'Could not load sample', { tone: 'err' });
    } finally {
      buttons.forEach((b) => { b.disabled = false; });
    }
  }

  function refreshRunbar() {
    if (input) {
      summaryTitle.textContent = input.name;
      summaryMeta.textContent = `${fmtBytes(input.stats.bytes)} · ${fmtInt(input.stats.bases)} bases · ${databases.find((d) => d.id === settings.db)?.label || settings.db}`;
    } else {
      summaryTitle.textContent = 'No genome loaded';
      summaryMeta.textContent = 'Drop a FASTA or pick a demo to begin';
    }
    runBtn.disabled = !input || running;
  }

  // --------------------------------------------------------------- settings

  root.querySelectorAll('input[name="db"]').forEach((radio) => {
    radio.addEventListener('change', () => {
      settings.db = radio.value;
      ctx.saveSettings(settings);
      refreshRunbar();
    });
  });

  $('#opt-fuzzy').addEventListener('change', (e) => {
    settings.fuzzy = e.target.checked;
    ctx.saveSettings(settings);
  });
  $('#opt-async').addEventListener('change', (e) => {
    settings.async = e.target.checked;
    ctx.saveSettings(settings);
  });

  const range = $('#conf-range');
  const confValue = $('#conf-value');
  const paintRange = () => {
    const min = Number(range.min);
    const max = Number(range.max);
    const value = Number(range.value);
    range.style.setProperty('--fill', `${((value - min) / (max - min)) * 100}%`);
  };
  paintRange();
  range.addEventListener('input', () => {
    settings.confidence = Number(range.value);
    confValue.textContent = `${settings.confidence}%`;
    paintRange();
  });
  range.addEventListener('change', () => ctx.saveSettings(settings));

  // ------------------------------------------------------------- mode tabs

  root.querySelectorAll('[data-mode]').forEach((tab) => {
    tab.addEventListener('click', () => {
      if (running) return;
      mode = tab.dataset.mode;
      root.querySelectorAll('[data-mode]').forEach((t) => t.setAttribute('aria-selected', String(t === tab)));
      renderInput();
    });
  });

  // --------------------------------------------------------------- running

  function startTimer() {
    startedAt = performance.now();
    runTimer.textContent = '00:00.0';
    timer = setInterval(() => {
      const elapsed = performance.now() - startedAt;
      const total = elapsed / 1000;
      const mm = String(Math.floor(total / 60)).padStart(2, '0');
      const ss = String(Math.floor(total % 60)).padStart(2, '0');
      const tenths = Math.floor((total * 10) % 10);
      runTimer.textContent = `${mm}:${ss}.${tenths}`;
    }, 100);
  }

  function stopTimer() {
    if (timer) clearInterval(timer);
    timer = null;
  }

  function setRunning(value) {
    running = value;
    runBtn.disabled = value || !input;
    runBtn.innerHTML = value ? `<span class="boot__mark" style="width:14px;height:14px;border-width:1.5px"></span> Scanning…` : `${icon('scan')} Run scan`;
    cancelBtn.hidden = !value;
    root.querySelectorAll('input, textarea, [data-mode], [data-sample]').forEach((el) => {
      if (el === runBtn || el === cancelBtn) return;
      if ('disabled' in el) el.disabled = value;
    });
    if (!value) {
      root.querySelectorAll('[data-sample]').forEach((b) => { b.disabled = false; });
    }
  }

  async function runScan() {
    if (!input || running) return;
    setRunning(true);
    logs = [];
    lastOutcome = null;
    resetStages();
    setStage('validate', 'done', `${fmtInt(input.stats.bases)} bases`);
    setStage('index', 'active');
    clearConsole('Starting engine…');
    feedback.innerHTML = '';
    resultBox.hidden = true;
    runDot.className = 'dot dot--warn dot--live';
    startTimer();
    controller = new AbortController();

    const payload = {
      fastaContent: input.content,
      fileName: input.name,
      db: settings.db,
      fuzzy: Boolean(settings.fuzzy),
      async: settings.async !== false,
      confidence: Number(settings.confidence),
    };

    try {
      await streamScan(payload, {
        signal: controller.signal,
        onEvent: (event) => {
          if (event.type === 'stage') {
            setStage(event.id, event.status, event.detail);
          } else if (event.type === 'log') {
            appendLog(event.line);
          } else if (event.type === 'result') {
            lastOutcome = event.data;
          } else if (event.type === 'error') {
            throw Object.assign(new Error(event.message || 'Scan failed'), { detail: event.detail, hint: event.hint });
          }
        },
      });

      if (!lastOutcome) throw new Error('The engine finished without returning a report');
      STAGES.forEach((s) => setStage(s.id, 'done'));
      runDot.className = 'dot dot--ok';
      ctx.setLastResult(lastOutcome);
      renderOutcome(lastOutcome);
      const fail = lastOutcome.summary?.likelyToFail || 0;
      toast(fail > 0 ? `Resistance detected in ${fail} drug class${fail === 1 ? '' : 'es'}` : 'Scan complete — no resistance calls', { title: 'Report ready' });
      setTimeout(() => ctx.navigate('/report'), 700);
    } catch (err) {
      stopTimer();
      if (err.name === 'AbortError') {
        appendLog('[!] Scan cancelled by user');
        const activeStage = STAGES.find((s) => stageState[s.id]?.status === 'active');
        if (activeStage) setStage(activeStage.id, 'error', 'cancelled');
        feedback.innerHTML = `<div class="error-box"><b>${icon('x')} Scan cancelled</b><p>The engine process was stopped. Adjust the input and run again.</p></div>`;
        runDot.className = 'dot';
      } else {
        try { controller?.abort(); } catch (_) { /* already closed */ }
        appendLog(`[!] ${err.message}`);
        const active = STAGES.find((s) => stageState[s.id]?.status === 'active');
        if (active) setStage(active.id, 'error');
        feedback.innerHTML = `
          <div class="error-box">
            <b>${icon('alert')} ${esc(err.message || 'Scan failed')}</b>
            ${err.hint ? `<p>${esc(err.hint)}</p>` : ''}
            ${err.detail ? `<code>${esc(err.detail)}</code>` : ''}
            <p>Check that the engine binary exists (<code>make</code>) and the catalogue is present in <code>data/</code>.</p>
          </div>`;
        runDot.className = 'dot dot--err';
        toast(err.message || 'Scan failed', { tone: 'err', title: 'Engine error' });
        stopTimer();
      }
    } finally {
      setRunning(false);
      controller = null;
      if (timer) stopTimer();
    }
  }

  function renderOutcome(result) {
    const t = result.telemetry || {};
    const s = result.summary || {};
    resultBox.hidden = false;
    resultBox.innerHTML = `
      <div class="mini-metrics">
        <div class="mini-metric"><span>Scan</span><b>${fmtMs(t.scanTimeMs)}</b></div>
        <div class="mini-metric"><span>Index</span><b>${fmtMs(t.dbLoadTimeMs)}</b></div>
        <div class="mini-metric"><span>Exact hits</span><b>${fmtInt(s.exactHits)}</b></div>
        <div class="mini-metric"><span>SNP hits</span><b>${fmtInt(s.mutatedHits)}</b></div>
      </div>
      <a class="btn btn--primary btn--block" style="margin-top:12px" href="#/report" data-nav="/report">
        ${icon('file')} Open full report
      </a>`;
  }

  runBtn.addEventListener('click', runScan);
  cancelBtn.addEventListener('click', () => controller?.abort());

  root.addEventListener('keydown', (e) => {
    if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
      e.preventDefault();
      runScan();
    }
  });

  root.querySelectorAll('[data-sample]').forEach((chip) => {
    chip.addEventListener('click', () => loadSample(chip.dataset.sample));
  });

  // -------------------------------------------------------------- lifecycle

  resetStages();
  renderInput();
  refreshRunbar();

  // A demo can be queued from the overview screen.
  const pending = ctx.consumePendingSample?.();
  if (pending) {
    loadSample(pending);
  }

  root.__cleanup = () => {
    stopTimer();
    controller?.abort();
  };

  return root;
}