import { icon, esc, fmtInt, fmtMs } from '../ui.js';

function stat(value, label, accent = false) {
  return `<div class="hero__stat"><b>${accent ? `<em>${value}</em>` : value}</b><span>${label}</span></div>`;
}

function feature(iconName, title, body, left, right) {
  return `
    <article class="panel feature">
      <div class="feature__icon">${icon(iconName)}</div>
      <h3>${title}</h3>
      <p>${body}</p>
      <div class="feature__meta"><span>${left}</span><b>${right}</b></div>
    </article>`;
}

export function renderOverview(state) {
  const health = state.health;
  const recommended = health?.databases?.find((d) => d.recommended) || health?.databases?.[0];
  const markers = recommended?.markers ? fmtInt(recommended.markers) : '455';
  const classes = recommended?.classes?.length || 11;
  const cores = health?.host?.cpus || navigator.hardwareConcurrency || 4;
  const lastScan = state.lastResult;

  const heroStats = [
    stat(markers, 'reference markers'),
    stat(String(classes), 'drug classes'),
    stat(String(cores), 'native threads', true),
  ].join('');

  const root = document.createElement('div');

  root.innerHTML = `
    <section class="container hero">
      <div class="hero__copy">
        <span class="eyebrow">Antimicrobial resistance · early warning</span>
        <h1 class="hero__title">Read the genome.<br><span class="dim">Know the</span> <span class="signal">resistance.</span></h1>
        <p class="lead hero__lead">
          Scan raw bacterial FASTA against curated CARD resistance markers, tolerate single-nucleotide
          variants, and get a calibrated probability for every drug class — in milliseconds, on your own machine.
        </p>
        <div class="hero__cta">
          <a class="btn btn--primary" href="#/scan" data-nav="/scan">
            ${icon('play')} Run an analysis
          </a>
          <button class="btn" type="button" data-action="demo" data-sample="sample_real.fasta">
            ${icon('flask')} Load demo genome
          </button>
        </div>
        <div class="hero__hint">
          ${icon('lock')} 100% local inference · no sequence leaves this device
        </div>
        <div class="hero__stats">${heroStats}</div>
      </div>

      <aside class="panel pipeline reveal" aria-label="Engine pipeline">
        <div class="pipeline__head">
          <span class="pipeline__title">Inference pipeline</span>
          <span class="pill pill--mint"><span class="dot dot--ok"></span> ready</span>
        </div>
        <div class="pipeline__steps">
          <div class="pipeline__step">
            <span class="pipeline__index">01</span>
            <div><div class="pipeline__name">Zero-copy ingest</div><div class="pipeline__desc">memory-mapped FASTA, 64 KB chunks</div></div>
            <span class="pipeline__meta">mmap</span>
          </div>
          <div class="pipeline__step">
            <span class="pipeline__index">02</span>
            <div><div class="pipeline__name">Trie index + Bloom</div><div class="pipeline__desc">4-ary prefix tree, 1 M-bit pre-filter</div></div>
            <span class="pipeline__meta">${markers} mk</span>
          </div>
          <div class="pipeline__step">
            <span class="pipeline__index">03</span>
            <div><div class="pipeline__name">Parallel 21-mer scan</div><div class="pipeline__desc">Hamming ≤ 1 mutation tolerance</div></div>
            <span class="pipeline__meta">${cores} thr</span>
          </div>
          <div class="pipeline__step">
            <span class="pipeline__index">04</span>
            <div><div class="pipeline__name">Logistic inference</div><div class="pipeline__desc">P = σ(WᵀX + b) per drug class</div></div>
            <span class="pipeline__meta">embedded</span>
          </div>
          <div class="pipeline__step">
            <span class="pipeline__index">05</span>
            <div><div class="pipeline__name">Tiered report</div><div class="pipeline__desc">evidence I–III + recommendation</div></div>
            <span class="pipeline__meta">JSON</span>
          </div>
        </div>
        <div class="genome-strip" aria-hidden="true">
          <div class="genome-strip__letters">ACGTTGCAAGCTTAGCGATCG<b>ATCGGATC</b>GCTAGCTTACGATCGATCGGCTAGCTAGCGATGCTAGCTAGCTAGCGATCGATCG</div>
          <div class="genome-strip__scan"></div>
          <div class="genome-strip__label"><span>k-mer window · 21 bp</span><span>Hamming ≤ 1</span></div>
        </div>
      </aside>
    </section>

    <section class="container section">
      <div class="feature-grid">
        ${feature('layers', 'Zero-copy ingestion',
          'Genomes are memory-mapped and streamed in 64 KB chunks with a 20-base overlap, so markers never fall through a chunk boundary. Auxiliary RAM stays flat on multi-gigabyte files.',
          'FastaMmapParser', 'O(1) RAM')}
        ${feature('activity', 'Mutation-tolerant matching',
          'A 4-ary prefix tree backtracks through one mismatch, so a single SNP inside a resistance gene does not hide it. A Bloom filter rejects negatives in constant time first.',
          'AMRTrie · Bloom', 'Hamming ≤ 1')}
        ${feature('cpu', 'Embedded inference',
          'Logistic-regression weights are compiled into the binary and evaluated with raw dot products. No Python runtime, no network calls, no model download at scan time.',
          'LogisticRegressor', 'σ(WᵀX + b)')}
      </div>
    </section>

    <section class="container section">
      <div class="section-head">
        <div class="section-head__text">
          <span class="eyebrow">Workflow</span>
          <h2>From raw reads to a clinical signal.</h2>
          <p class="lead">Four steps, one local process. The engine never phones home.</p>
        </div>
        <a class="btn" href="#/scan" data-nav="/scan">Start a scan ${icon('arrow-right')}</a>
      </div>
      <div class="steps">
        <article class="step">
          <span class="step__num">01 / INPUT</span>
          <h3>Load a genome</h3>
          <p>Drop a bacterial FASTA, paste a sequence, or start from one of the bundled demo genomes.</p>
        </article>
        <article class="step">
          <span class="step__num">02 / CONFIGURE</span>
          <h3>Choose a catalogue</h3>
          <p>CARD top markers, the extended panel, or the 10-marker core set. Toggle SNP tolerance and threading.</p>
        </article>
        <article class="step">
          <span class="step__num">03 / SCAN</span>
          <h3>Run the engine</h3>
          <p>Markers are indexed into a trie, then every 21-mer in the genome is matched across all cores.</p>
        </article>
        <article class="step">
          <span class="step__num">04 / REPORT</span>
          <h3>Read the signal</h3>
          <p>Per-class probability, confidence, evidence tier and a recommendation you can export as JSON or CSV.</p>
        </article>
      </div>
    </section>

    <section class="container section section--tight">
      <div class="tier-grid">
        <article class="panel tier tier--1">
          <span class="tier__label">Tier I</span>
          <h3>Direct marker match</h3>
          <p>A validated AMR gene or point mutation was found in the genome. Strongest resistance evidence.</p>
        </article>
        <article class="panel tier tier--2">
          <span class="tier__label">Tier II</span>
          <h3>Statistical association</h3>
          <p>No single decisive marker, but the multi-marker profile shifts the model probability. Treat with caution.</p>
        </article>
        <article class="panel tier tier--3">
          <span class="tier__label">Tier III</span>
          <h3>Susceptibility signal</h3>
          <p>No resistance markers detected and the model probability stays low. A viable target — pending lab confirmation.</p>
        </article>
      </div>
    </section>

    <section class="container section">
      <div class="panel cta">
        <div class="cta__copy">
          <span class="eyebrow">${lastScan ? 'Pick up where you left off' : 'Ready when you are'}</span>
          <h2>${lastScan ? 'Your last report is still here.' : 'Have a genome handy?'}</h2>
          <p class="lead">
            ${lastScan
              ? `${esc(lastScan.input?.fileName || 'Genome')} was analysed against ${esc(lastScan.database?.file || 'the reference catalogue')} · ${fmtMs(lastScan.telemetry?.scanTimeMs)} scan time.`
              : 'Start with the bundled multidrug-resistant E. coli demo, or drop your own FASTA. The first scan takes a few seconds to index the catalogue.'}
          </p>
        </div>
        <div class="cta__actions">
          ${lastScan ? `<a class="btn" href="#/report" data-nav="/report">${icon('file')} Open last report</a>` : ''}
          <button class="btn btn--primary" type="button" data-action="demo" data-sample="sample_real.fasta">
            ${icon('play')} ${lastScan ? 'Run another scan' : 'Run the demo'}
          </button>
        </div>
      </div>
    </section>
  `;

  return root;
}