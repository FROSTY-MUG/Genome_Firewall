import {
  icon, esc, fmtInt, fmtBytes, fmtMs, statusMeta, tierMeta, copyText, download, toast, timeAgo,
} from '../ui.js';

function csvEscape(value) {
  const text = String(value ?? '');
  return /[",\n]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text;
}

export function buildCsv(result) {
  const rows = [
    ['drug_class', 'status', 'probability', 'confidence_percent', 'evidence_tier', 'direct_hits', 'mutated_hits', 'target_present', 'recommendation'],
    ...result.predictions.map((p) => [
      p.drugName,
      p.status,
      p.probability,
      p.confidenceScore,
      String(p.evidenceTier).replace(/[[\]]/g, ''),
      p.directHits,
      p.mutatedHits,
      p.targetPresent ? 'yes' : 'no',
      p.clinicalRecommendation,
    ]),
  ];
  return rows.map((row) => row.map(csvEscape).join(',')).join('\n');
}

export function buildSummary(result) {
  const s = result.summary || {};
  const lines = [
    'GENOME FIREWALL v2.0 — AMR DECISION REPORT',
    `Genome:    ${result.input?.fileName || 'unknown'}`,
    `Catalogue: ${result.request?.databaseLabel || result.database?.file || '—'} (${fmtInt(result.database?.markers)} markers)`,
    `Mode:      ${result.engine?.systemMode === 'exact' ? 'exact match' : 'fuzzy (Hamming ≤ 1)'} · ${result.engine?.threading === 'serial' ? 'single-threaded' : 'multithreaded'}`,
    `Scanned:   ${fmtInt(result.input?.bases)} bases in ${fmtMs(result.telemetry?.scanTimeMs)}`,
    `Calls:     ${s.likelyToFail || 0} resistant · ${s.noCall || 0} no-call · ${s.likelyToWork || 0} susceptible`,
    '',
    'Drug class                 Verdict          P(res)   Confidence  Tier',
    '─'.repeat(74),
    ...result.predictions.map((p) => {
      const st = statusMeta(p.status);
      return `${p.drugName.padEnd(26)} ${st.label.padEnd(15)} ${Number(p.probability).toFixed(4).padStart(7)} ${(Number(p.confidenceScore).toFixed(1) + '%').padStart(10)}  ${String(p.evidenceTier).replace(/[[\]]/g, '')}`;
    }).join('\n'),
    '',
    'Defensive decision support only — confirm with standard laboratory AST before clinical use.',
  ];
  return lines.join('\n');
}

function explanation(pred) {
  const tier = tierMeta(pred.evidenceTier);
  const p = Number(pred.probability).toFixed(4);
  if (tier.key === 1) {
    return `Tier I: ${fmtInt(pred.directHits)} exact 21-mer window${pred.directHits === 1 ? '' : 's'} matched a validated resistance marker for this class${pred.mutatedHits ? `, plus ${fmtInt(pred.mutatedHits)} SNP-tolerant hits` : ''}. The embedded model scores P = ${p}.`;
  }
  if (tier.key === 2) {
    return `Tier II: no single decisive marker was found, so the call comes from the multi-marker feature profile. The model scores P = ${p}${pred.targetPresent ? ' with an intact molecular target detected' : ''}. Treat this as a statistical association, not a confirmed mechanism.`;
  }
  return `Tier III: no resistance signal was detected for this class and the model probability stayed at P = ${p}. ${pred.targetPresent ? 'An intact molecular target sequence was detected.' : 'No target sequence was resolved in this run.'} Confirm with phenotypic susceptibility testing.`;
}

export function renderReport(state, ctx) {
  const result = state.lastResult;
  const root = document.createElement('div');

  if (!result || !Array.isArray(result.predictions)) {
    root.innerHTML = `
      <div class="container">
        <div class="page-head">
          <div class="page-head__text">
            <a class="back-link" href="#/" data-nav="/">${icon('chevron-left')} Overview</a>
            <h1 class="page-head__title">No report yet</h1>
            <p class="lead">Run an analysis and the decision report will appear here.</p>
          </div>
        </div>
        <div class="panel empty-results" style="padding:56px 20px">
          <span class="run-empty__glyph">${icon('file')}</span>
          <b>Nothing has been analysed on this device</b>
          <span>Load the bundled MDR E. coli demo, or bring your own FASTA.</span>
          <div style="display:flex;gap:8px;flex-wrap:wrap;justify-content:center;margin-top:12px">
            <a class="btn btn--primary" href="#/scan" data-nav="/scan">${icon('play')} Start a scan</a>
            <button class="btn" type="button" data-action="demo" data-sample="sample_real.fasta">${icon('flask')} Run the demo</button>
          </div>
        </div>
      </div>`;
    return root;
  }

  const summary = result.summary || {};
  const fail = summary.likelyToFail || 0;
  const noCall = summary.noCall || 0;
  const work = summary.likelyToWork || 0;
  const total = summary.drugCount || result.predictions.length;
  const verdictTone = fail > 0 ? 'fail' : noCall > 0 ? 'warn' : 'work';
  const verdictTitle = fail > 0 ? 'Resistance detected' : noCall > 0 ? 'Inconclusive profile' : 'No resistance detected';
  const verdictSub = fail > 0
    ? `${fail} of ${total} drug classes show a resistance call against ${esc(result.request?.databaseLabel || result.database?.file || 'the reference catalogue')}. Review the Tier I rows first.`
    : noCall > 0
      ? `No drug class cleared the resistance threshold, but ${noCall} of ${total} could not be called confidently. The profile is not clean.`
      : `No resistance markers were detected across ${total} drug classes. All calls are susceptibility signals pending laboratory confirmation.`;
  const timestamp = result.request?.scannedAt || new Date().toISOString();
  const dbLabel = result.request?.databaseLabel || result.database?.file || 'catalogue';
  const t = result.telemetry || {};
  const inputStats = result.input || {};

  root.innerHTML = `
    <div class="container">
      <div class="page-head">
        <div class="page-head__text">
          <a class="back-link" href="#/scan" data-nav="/scan">${icon('chevron-left')} New analysis</a>
          <h1 class="page-head__title">Decision report</h1>
          <p class="lead" style="font-size:13px">
            <span class="mono">${esc(inputStats.fileName || 'genome.fasta')}</span> ·
            ${esc(dbLabel)} ·
            ${esc(new Date(timestamp).toLocaleString())} ·
            ${timeAgo(timestamp)}
          </p>
        </div>
        <div class="page-head__actions">
          <button class="btn" type="button" data-export="csv">${icon('download')} CSV</button>
          <button class="btn" type="button" data-export="json">${icon('download')} JSON</button>
          <a class="btn" href="#/scan" data-nav="/scan">${icon('plus')} New scan</a>
        </div>
      </div>

      <section class="verdict verdict--${verdictTone}" aria-label="Verdict">
        <div class="verdict__copy">
          <span class="eyebrow">Verdict · ${esc(dbLabel)}</span>
          <h2 class="verdict__title">${verdictTitle}</h2>
          <p class="verdict__sub">${verdictSub}</p>
        </div>
        <div class="verdict__counts">
          <div class="verdict__count verdict__count--fail"><b>${fail}</b><span>resistant</span></div>
          <div class="verdict__count verdict__count--warn"><b>${noCall}</b><span>no-call</span></div>
          <div class="verdict__count verdict__count--work"><b>${work}</b><span>susceptible</span></div>
        </div>
      </section>

      <div class="metric-strip">
        <div class="metric"><span>Total wall clock</span><b>${fmtMs(t.totalTimeMs)}</b></div>
        <div class="metric"><span>Inference</span><b>${fmtMs(t.scanTimeMs)}</b></div>
        <div class="metric"><span>Throughput</span><b>${Number(t.throughputMBps || 0) >= 0.01 ? `${Number(t.throughputMBps).toFixed(2)} <small>MB/s</small>` : `${(Number(t.throughputMBps || 0) * 1024).toFixed(1)} <small>KB/s</small>`}</b></div>
        <div class="metric"><span>Sequence scanned</span><b>${fmtBytes(t.totalBytesScanned)} <small>${fmtInt(inputStats.bases)} bases</small></b></div>
      </div>

      <div class="report-grid">
        <section class="panel results-panel">
          <div class="results-toolbar">
            <div class="filters" role="group" aria-label="Filter by verdict">
              <button class="filter is-active" type="button" data-filter="all">All <b>${total}</b></button>
              <button class="filter" type="button" data-filter="fail">Resistant <b>${fail}</b></button>
              <button class="filter" type="button" data-filter="warn">No-call <b>${noCall}</b></button>
              <button class="filter" type="button" data-filter="work">Susceptible <b>${work}</b></button>
            </div>
            <div class="search">
              ${icon('search')}
              <input type="search" id="report-search" placeholder="Filter drug class…" aria-label="Filter drug classes" />
            </div>
            <select class="sort" id="report-sort" aria-label="Sort results">
              <option value="resistance">Highest resistance</option>
              <option value="resistance-asc">Lowest resistance</option>
              <option value="confidence">Confidence</option>
              <option value="name">Drug A–Z</option>
            </select>
          </div>

          <div class="table-wrap">
            <table class="results-table">
              <thead>
                <tr>
                  <th scope="col">Drug class</th>
                  <th scope="col">Verdict</th>
                  <th scope="col">P(resistance)</th>
                  <th scope="col" class="num">Confidence</th>
                  <th scope="col">Evidence</th>
                  <th scope="col" class="num">Hits e/s</th>
                  <th scope="col"><span class="faint" aria-hidden="true">·</span></th>
                </tr>
              </thead>
              <tbody id="report-body"></tbody>
            </table>
          </div>
        </section>

        <aside class="report-aside">
          <section class="panel aside-card">
            <div class="aside-card__head">
              <span class="aside-card__title">${icon('activity')} Run telemetry</span>
              <span class="pill">${esc(result.engine?.systemMode === 'exact' ? 'exact' : 'fuzzy')} · ${esc(result.engine?.threading === 'serial' ? 'serial' : 'async')}</span>
            </div>
            <div class="kv">
              <div class="kv__row"><span>Catalogue</span><b>${esc(result.database?.file || '—')}</b></div>
              <div class="kv__row"><span>Markers indexed</span><b>${fmtInt(result.database?.markers)}</b></div>
              <div class="kv__row"><span>Trie nodes</span><b>${fmtInt(result.database?.trieNodes)}</b></div>
              <div class="kv__row"><span>Index memory</span><b>${fmtInt(result.database?.trieMemoryKB)} KB</b></div>
              <div class="kv__row"><span>Bloom filter</span><b>${fmtInt(result.database?.bloomBits)} bits</b></div>
              <div class="kv__row"><span>Chunks</span><b>${fmtInt(t.chunkCount)} × ${fmtInt(result.engine?.chunkSize)} B</b></div>
              <div class="kv__row"><span>Exact / SNP hits</span><b>${fmtInt(summary.exactHits)} / ${fmtInt(summary.mutatedHits)}</b></div>
              <div class="kv__row"><span>Confidence gate</span><b>${fmtInt(result.engine?.confidenceThreshold)}%</b></div>
            </div>
          </section>

          <section class="panel aside-card">
            <div class="aside-card__head">
              <span class="aside-card__title">${icon('layers')} Evidence tiers</span>
            </div>
            <div class="legend">
              <div class="legend__item"><span class="legend__bar legend__bar--1"></span><div><b>Tier I · direct</b><span>Validated gene or point mutation matched.</span></div></div>
              <div class="legend__item"><span class="legend__bar legend__bar--2"></span><div><b>Tier II · modelled</b><span>Statistical signal from the full marker profile.</span></div></div>
              <div class="legend__item"><span class="legend__bar legend__bar--3"></span><div><b>Tier III · intact</b><span>No resistance signal; target looks viable.</span></div></div>
            </div>
          </section>

          <section class="panel aside-card">
            <div class="aside-card__head">
              <span class="aside-card__title">${icon('download')} Export</span>
            </div>
            <div class="export-btns">
              <button class="btn" type="button" data-export="json">${icon('file')} JSON</button>
              <button class="btn" type="button" data-export="csv">${icon('file')} CSV</button>
              <button class="btn" type="button" data-export="copy" style="grid-column:1/-1">${icon('copy')} Copy summary</button>
            </div>
            <p class="disclaimer"><b>Research use only.</b> Genome Firewall is decision support, not a diagnostic device. Confirm every call with standard laboratory AST.</p>
          </section>
        </aside>
      </div>
    </div>
  `;

  const body = root.querySelector('#report-body');
  const search = root.querySelector('#report-search');
  const sort = root.querySelector('#report-sort');
  let filter = 'all';
  let query = '';
  let sortMode = 'resistance';
  const expanded = new Set();

  function visibleRows() {
    let rows = result.predictions.slice();
    if (filter !== 'all') rows = rows.filter((p) => statusMeta(p.status).key === filter);
    if (query) {
      const q = query.toLowerCase();
      rows = rows.filter((p) => p.drugName.toLowerCase().includes(q));
    }
    if (sortMode === 'resistance') rows.sort((a, b) => b.probability - a.probability);
    if (sortMode === 'resistance-asc') rows.sort((a, b) => a.probability - b.probability);
    if (sortMode === 'confidence') rows.sort((a, b) => b.confidenceScore - a.confidenceScore);
    if (sortMode === 'name') rows.sort((a, b) => a.drugName.localeCompare(b.drugName));
    return rows;
  }

  function rowHtml(p) {
    const st = statusMeta(p.status);
    const tier = tierMeta(p.evidenceTier);
    const isOpen = expanded.has(p.drugName);
    return `
      <tr class="row ${isOpen ? 'is-open' : ''}" data-drug="${esc(p.drugName)}" tabindex="0" aria-expanded="${isOpen}">
        <td class="cell-drug">${esc(p.drugName)}</td>
        <td class="cell-verdict"><span class="pill ${st.cls}">${st.label}</span></td>
        <td class="cell-prob" data-label="P(resistance)">
          <div class="prob-cell">
            <span class="prob-num">${Number(p.probability).toFixed(4)}</span>
            <div class="prob-track"><div class="prob-fill prob-fill--${st.key}" style="width:${Math.max(1, Number(p.probability) * 100).toFixed(1)}%"></div></div>
          </div>
        </td>
        <td class="num cell-conf" data-label="Confidence"><span class="conf">${Number(p.confidenceScore).toFixed(1)}%</span></td>
        <td class="cell-tier" data-label="Evidence"><span class="tier-tag" style="color:${tier.color}">${tier.label}</span></td>
        <td class="num cell-hits" data-label="Hits e/s"><span class="hits">${fmtInt(p.directHits)}<i>/${fmtInt(p.mutatedHits)}</i></span></td>
        <td class="cell-chev">${icon('chevron-down', 'chev')}</td>
      </tr>
      <tr class="detail-row" ${isOpen ? '' : 'hidden'} data-detail="${esc(p.drugName)}">
        <td colspan="7">
          <div class="detail">
            <div class="detail__cell"><span>Direct hits</span><b>${fmtInt(p.directHits)}</b></div>
            <div class="detail__cell"><span>SNP-tolerant hits</span><b>${fmtInt(p.mutatedHits)}</b></div>
            <div class="detail__cell"><span>Target sequence</span><b>${p.targetPresent ? 'detected' : 'not resolved'}</b></div>
            <div class="detail__cell"><span>Recommendation</span><b style="color:${tier.color}">${esc(p.clinicalRecommendation)}</b></div>
            <div class="detail__note">${icon('info')}<span>${esc(explanation(p))}</span></div>
          </div>
        </td>
      </tr>`;
  }

  function renderRows() {
    const rows = visibleRows();
    if (!rows.length) {
      body.innerHTML = `<tr><td colspan="7"><div class="empty-results"><b>No drug classes match this filter</b><span>Try a different verdict or clear the search.</span></div></td></tr>`;
      return;
    }
    body.innerHTML = rows.map(rowHtml).join('');
    body.querySelectorAll('tr.row').forEach((row) => {
      const toggle = () => {
        const drug = row.dataset.drug;
        if (expanded.has(drug)) expanded.delete(drug); else expanded.add(drug);
        renderRows();
      };
      row.addEventListener('click', toggle);
      row.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); toggle(); }
      });
    });
  }

  root.querySelectorAll('[data-filter]').forEach((btn) => {
    btn.addEventListener('click', () => {
      filter = btn.dataset.filter;
      root.querySelectorAll('[data-filter]').forEach((b) => b.classList.toggle('is-active', b === btn));
      renderRows();
    });
  });

  search.addEventListener('input', () => { query = search.value.trim(); renderRows(); });
  sort.addEventListener('change', () => { sortMode = sort.value; renderRows(); });

  root.querySelectorAll('[data-export]').forEach((btn) => {
    btn.addEventListener('click', async () => {
      const kind = btn.dataset.export;
      const base = String(inputStats.fileName || 'genome').replace(/\.[^.]+$/, '');
      if (kind === 'json') {
        download(`genome-firewall-${base}.json`, JSON.stringify(result, null, 2), 'application/json');
        toast('JSON report downloaded');
      } else if (kind === 'csv') {
        download(`genome-firewall-${base}.csv`, buildCsv(result), 'text/csv');
        toast('CSV report downloaded');
      } else {
        const ok = await copyText(buildSummary(result));
        toast(ok ? 'Report summary copied to clipboard' : 'Could not access the clipboard', { tone: ok ? 'ok' : 'err' });
      }
    });
  });

  renderRows();

  root.__cleanup = () => {};
  return root;
}