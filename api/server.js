#!/usr/bin/env node
/**
 * Genome Firewall — local web API + static host.
 *
 * Dependency-free on purpose: it starts instantly on Termux/Android, in
 * containers, or on a laptop without an npm install. It is a thin, defensive
 * wrapper around the compiled `genome_firewall` binary.
 *
 *   node api/server.js            -> http://127.0.0.1:8787
 *   PORT=9000 HOST=0.0.0.0 node api/server.js
 */

'use strict';

const http = require('http');
const fs = require('fs');
const fsp = require('fs/promises');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const { spawn } = require('child_process');

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

const ROOT = path.resolve(__dirname, '..');
const WEB_DIR = path.join(ROOT, 'web');
const DATA_DIR = path.join(ROOT, 'data');
const BINARY = path.join(ROOT, 'genome_firewall');
const PORT = Number(process.env.PORT || 8787);
const HOST = process.env.HOST || '127.0.0.1';
const MAX_UPLOAD_BYTES = Number(process.env.MAX_UPLOAD_MB || 40) * 1024 * 1024;
const SCAN_TIMEOUT_MS = Number(process.env.SCAN_TIMEOUT_MS || 180000);
const ENGINE_VERSION = '2.0';

// Reference catalogues exposed to the UI. Anything not on this list is
// rejected by the API, so the browser can never point the engine at an
// arbitrary file.
const DATABASE_CATALOG = [
  {
    id: 'markers_card_top.csv',
    label: 'CARD Top Markers',
    short: 'CARD',
    description: '455 curated CARD homologues across 11 drug classes. Recommended for real genomes.',
    recommended: true,
  },
  {
    id: 'markers_large.csv',
    label: 'Extended Panel',
    short: 'EXT',
    description: 'Multi-marker signatures spanning 10 drug classes from NCBI, CARD and ResFinder.',
    recommended: false,
  },
  {
    id: 'markers.csv',
    label: 'Core Panel',
    short: 'CORE',
    description: '10 hand-curated markers. Fastest to index — ideal for demos and smoke tests.',
    recommended: false,
  },
];

const SAMPLE_CATALOG = [
  {
    id: 'sample_real.fasta',
    label: 'MDR E. coli (demo)',
    description: 'Multidrug-resistant E. coli K88 with 14 embedded AMR fragments, including SNP variants.',
    badge: 'Recommended',
  },
  {
    id: 'sample_large.fasta',
    label: 'Stress genome',
    description: 'Larger synthetic chromosome for timing the multithreaded scanner.',
    badge: 'Performance',
  },
  {
    id: 'sample.fasta',
    label: 'E. coli K12 (baseline)',
    description: 'Sensitive reference genome with no embedded resistance cassettes.',
    badge: 'Baseline',
  },
];

// ---------------------------------------------------------------------------
// Small utilities
// ---------------------------------------------------------------------------

function log(...args) {
  const stamp = new Date().toISOString().slice(11, 19);
  console.log(`[${stamp}]`, ...args);
}

class HttpError extends Error {
  constructor(status, message, hint) {
    super(message);
    this.status = status;
    this.hint = hint;
  }
}

function sendJson(res, status, body) {
  const payload = JSON.stringify(body);
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(payload),
    'Cache-Control': 'no-store',
  });
  res.end(payload);
}

function readBody(req, limit) {
  return new Promise((resolve, reject) => {
    let size = 0;
    const chunks = [];
    req.on('data', (chunk) => {
      size += chunk.length;
      if (size > limit) {
        reject(new HttpError(413, 'Upload too large', `Maximum request size is ${(limit / 1024 / 1024).toFixed(0)} MB.`));
        req.destroy();
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    req.on('error', reject);
  });
}

function safeName(name) {
  const base = path.basename(String(name || 'genome.fasta')).replace(/[\u0000-\u001f<>:"/\\|?*]/g, '_');
  return base.slice(0, 120) || 'genome.fasta';
}

function normalizeFasta(content) {
  // Accept CRLF, strip UTF-8 BOM, trim trailing whitespace.
  return content.replace(/^\uFEFF/, '').replace(/\r\n?/g, '\n').trim();
}

function inspectFasta(content) {
  const bytes = Buffer.byteLength(content, 'utf8');
  const lines = content.split('\n');
  const headers = lines.filter((l) => l.startsWith('>')).length;
  let bases = 0;
  let gc = 0;
  let ambiguous = 0;
  const counts = { A: 0, C: 0, G: 0, T: 0, N: 0 };
  for (const line of lines) {
    if (line.startsWith('>')) continue; // header letters are not sequence
    for (const ch of line) {
      const c = ch.toUpperCase();
      if (c === 'A' || c === 'C' || c === 'G' || c === 'T') {
        bases++;
        counts[c]++;
        if (c === 'G' || c === 'C') gc++;
      } else if (c === 'N') {
        ambiguous++;
      }
    }
  }
  return {
    bytes,
    headers,
    bases,
    ambiguous,
    gcPercent: bases > 0 ? Number(((gc / bases) * 100).toFixed(2)) : 0,
    composition: counts,
  };
}

function validateScanInput(body) {
  if (!body || typeof body !== 'object') throw new HttpError(400, 'Expected a JSON body');
  const db = body.db;
  const database = DATABASE_CATALOG.find((d) => d.id === db);
  if (!database) throw new HttpError(400, 'Unknown reference database', 'Pick one of the catalogues listed by /api/databases.');

  const content = normalizeFasta(String(body.fastaContent || ''));
  if (!content) throw new HttpError(400, 'No FASTA content supplied');
  if (Buffer.byteLength(content, 'utf8') > MAX_UPLOAD_BYTES) {
    throw new HttpError(413, 'FASTA file exceeds the upload limit', `Keep it under ${(MAX_UPLOAD_BYTES / 1024 / 1024).toFixed(0)} MB.`);
  }

  const stats = inspectFasta(content);
  if (stats.bases < 21) {
    throw new HttpError(422, 'Sequence too short to scan', 'A genome must contain at least 21 unambiguous A/C/G/T bases.');
  }

  const confidence = Number.isFinite(Number(body.confidence)) ? Math.min(100, Math.max(0, Number(body.confidence))) : 75;

  return {
    content,
    stats,
    fileName: safeName(body.fileName),
    database,
    fuzzy: body.fuzzy !== false,
    asyncMode: body.async !== false,
    confidence: Number(confidence.toFixed(2)),
  };
}

function buildArgs(inputPath, scan) {
  const args = ['--json', '--input', inputPath, '--db', path.join(DATA_DIR, scan.database.id)];
  if (!scan.fuzzy) args.push('--exact');
  if (!scan.asyncMode) args.push('--serial');
  if (scan.confidence !== 75) args.push('--confidence', String(scan.confidence));
  return args;
}

function parseEngineJson(stdout) {
  const trimmed = stdout.trim();
  if (!trimmed) throw new Error('Engine produced no output');
  // Tolerate stray diagnostics by taking the last JSON object line.
  const lines = trimmed.split('\n').filter(Boolean);
  for (let i = lines.length - 1; i >= 0; i--) {
    const line = lines[i].trim();
    if (line.startsWith('{') && line.endsWith('}')) {
      try {
        return JSON.parse(line);
      } catch (_) {
        /* keep looking */
      }
    }
  }
  try {
    return JSON.parse(trimmed);
  } catch (_) {
    throw new Error(`Could not parse engine output: ${trimmed.slice(0, 300)}`);
  }
}

/**
 * Spawn the engine. `hooks` receives raw stderr lines for live progress.
 * Returns a cancel() function and a promise for the final result.
 */
function runEngine(scan, hooks = {}) {
  const tmpFile = path.join(os.tmpdir(), `gf_${crypto.randomBytes(8).toString('hex')}.fasta`);
  fs.writeFileSync(tmpFile, scan.content, { mode: 0o600 });

  const args = buildArgs(tmpFile, scan);
  const child = spawn(BINARY, args, { cwd: ROOT });
  let stdout = '';
  let stderr = '';
  let cancelRequested = false;
  let timeout = null;

  const cleanup = () => {
    try { fs.unlinkSync(tmpFile); } catch (_) { /* already gone */ }
    if (timeout) clearTimeout(timeout);
  };

  const promise = new Promise((resolve) => {
    timeout = setTimeout(() => {
      cancelRequested = true;
      child.kill('SIGKILL');
      resolve({ ok: false, error: `Scan timed out after ${SCAN_TIMEOUT_MS / 1000}s` });
    }, SCAN_TIMEOUT_MS);
    timeout.unref?.();

    child.stdout.on('data', (d) => { stdout += d.toString(); });

    let buffer = '';
    child.stderr.on('data', (d) => {
      buffer += d.toString();
      const lines = buffer.split('\n');
      buffer = lines.pop() || '';
      for (const line of lines) {
        if (!line.trim()) continue;
        stderr += line + '\n';
        hooks.onLog?.(line);
      }
    });

    child.on('error', (err) => {
      cleanup();
      resolve({ ok: false, error: `Could not start the engine: ${err.message}`, hint: 'Build it with `make` first.' });
    });

    child.on('close', (code) => {
      cleanup();
      if (cancelRequested) {
        resolve({ ok: false, cancelled: true, error: 'Scan cancelled' });
        return;
      }
      if (code !== 0) {
        const tail = stderr.trim().split('\n').slice(-4).join('\n');
        resolve({ ok: false, error: `Engine exited with code ${code}`, detail: tail });
        return;
      }
      try {
        const data = parseEngineJson(stdout);
        if (data.error) {
          resolve({ ok: false, error: data.message || data.error });
          return;
        }
        data.input.fileName = scan.fileName;
        if (scan.stats) data.input = { ...data.input, ...scan.stats };
        data.request = {
          databaseLabel: scan.database.label,
          fuzzy: scan.fuzzy,
          async: scan.asyncMode,
          confidence: scan.confidence,
          scannedAt: new Date().toISOString(),
        };
        resolve({ ok: true, data });
      } catch (err) {
        resolve({ ok: false, error: err.message });
      }
    });
  });

  return {
    promise,
    cancel() {
      cancelRequested = true;
      try { child.kill('SIGTERM'); } catch (_) { /* noop */ }
    },
  };
}

// ---------------------------------------------------------------------------
// Catalogue metadata (marker counts + drug classes, cached by mtime)
// ---------------------------------------------------------------------------

const catalogCache = new Map();

function readCatalog(db) {
  const file = path.join(DATA_DIR, db.id);
  let stat;
  try {
    stat = fs.statSync(file);
  } catch (_) {
    return { ...db, available: false, markers: 0, classes: [] };
  }
  const cached = catalogCache.get(db.id);
  if (cached && cached.mtimeMs === stat.mtimeMs) return cached.value;

  let markers = 0;
  const classes = new Set();
  try {
    const text = fs.readFileSync(file, 'utf8');
    const lines = text.split('\n');
    for (let i = 1; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line) continue;
      markers++;
      // targetDrug is the 3rd CSV column. Marker sequences never contain commas.
      const parts = line.split(',');
      if (parts[2]) classes.add(parts[2].trim());
    }
  } catch (err) {
    log('Could not read catalogue', db.id, err.message);
  }

  const value = {
    ...db,
    available: true,
    markers,
    classes: [...classes].sort(),
    sizeBytes: stat.size,
  };
  catalogCache.set(db.id, { mtimeMs: stat.mtimeMs, value });
  return value;
}

function healthPayload() {
  const databases = DATABASE_CATALOG.map(readCatalog);
  const totalMem = os.totalmem() / 1024 / 1024;
  return {
    status: fs.existsSync(BINARY) ? 'ok' : 'engine-missing',
    engine: {
      name: 'Genome Firewall',
      version: ENGINE_VERSION,
      binary: fs.existsSync(BINARY),
      path: path.relative(ROOT, BINARY),
    },
    host: {
      platform: process.platform,
      arch: process.arch,
      cpus: os.availableParallelism?.() || os.cpus().length || 1,
      totalMemMB: Math.round(totalMem),
      freeMemMB: Math.round(os.freemem() / 1024 / 1024),
      uptimeSec: Math.round(process.uptime()),
      node: process.version,
    },
    limits: { maxUploadMB: Math.round(MAX_UPLOAD_BYTES / 1024 / 1024), scanTimeoutSec: SCAN_TIMEOUT_MS / 1000 },
    databases,
    samples: SAMPLE_CATALOG.map((s) => {
      const p = path.join(DATA_DIR, s.id);
      let bytes = 0;
      try { bytes = fs.statSync(p).size; } catch (_) { /* missing */ }
      return { ...s, bytes, available: bytes > 0 };
    }),
  };
}

// ---------------------------------------------------------------------------
// Static file serving
// ---------------------------------------------------------------------------

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.woff2': 'font/woff2',
  '.txt': 'text/plain; charset=utf-8',
  '.fasta': 'text/plain; charset=utf-8',
  '.ico': 'image/x-icon',
};

function serveStatic(req, res, pathname) {
  let rel = decodeURIComponent(pathname);
  if (rel === '/' || rel === '') rel = '/index.html';

  const target = path.resolve(WEB_DIR, '.' + rel);
  if (!target.startsWith(WEB_DIR + path.sep) && target !== WEB_DIR) {
    return sendJson(res, 403, { error: 'Forbidden' });
  }

  fs.stat(target, (err, stat) => {
    if (err || !stat.isFile()) {
      // SPA-ish fallback for extension-less routes.
      if (!path.extname(rel)) return serveStatic(req, res, '/index.html');
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      return res.end('Not found');
    }
    const ext = path.extname(target).toLowerCase();
    res.writeHead(200, {
      'Content-Type': MIME[ext] || 'application/octet-stream',
      'Content-Length': stat.size,
      'Cache-Control': ext === '.html' ? 'no-cache' : 'public, max-age=3600',
    });
    fs.createReadStream(target).pipe(res);
  });
}

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------

async function handleScan(req, res, stream) {
  const raw = await readBody(req, MAX_UPLOAD_BYTES + 1024 * 1024);
  let body;
  try {
    body = JSON.parse(raw);
  } catch (_) {
    throw new HttpError(400, 'Body is not valid JSON');
  }

  const scan = validateScanInput(body);

  if (!stream) {
    const job = runEngine(scan);
    const outcome = await job.promise;
    if (!outcome.ok) return sendJson(res, 422, { error: outcome.error, detail: outcome.detail, hint: outcome.hint });
    return sendJson(res, 200, outcome.data);
  }

  // ---- NDJSON progress stream -------------------------------------------------
  res.writeHead(200, {
    'Content-Type': 'application/x-ndjson; charset=utf-8',
    'Cache-Control': 'no-cache, no-transform',
    Connection: 'keep-alive',
    'X-Accel-Buffering': 'no',
  });
  const emit = (event) => {
    if (!res.writableEnded) res.write(JSON.stringify(event) + '\n');
  };

  emit({ type: 'stage', id: 'validate', status: 'done', detail: `${scan.stats.bases.toLocaleString()} bases` });
  emit({ type: 'stage', id: 'index', status: 'active' });

  const job = runEngine(scan, {
    onLog(line) {
      emit({ type: 'log', line });
      if (line.includes('Indexed')) emit({ type: 'stage', id: 'index', status: 'active', detail: line.replace(/^\s*->\s*/, '') });
      if (line.includes('Load & Index Time')) {
        emit({ type: 'stage', id: 'index', status: 'done', detail: line.split(':').pop().trim() });
        emit({ type: 'stage', id: 'scan', status: 'active' });
      }
      if (line.includes('Scan completed')) {
        emit({ type: 'stage', id: 'scan', status: 'done', detail: line.replace(/^\[\+\]\s*/, '') });
        emit({ type: 'stage', id: 'infer', status: 'active' });
      }
    },
  });

  res.on('close', () => {
    if (!res.writableEnded) job.cancel();
  });

  const outcome = await job.promise;
  if (outcome.ok) {
    emit({ type: 'stage', id: 'infer', status: 'done' });
    emit({ type: 'stage', id: 'report', status: 'done', detail: `${outcome.data.predictions.length} drug classes scored` });
    emit({ type: 'result', data: outcome.data });
  } else if (!outcome.cancelled) {
    emit({ type: 'error', message: outcome.error, detail: outcome.detail, hint: outcome.hint });
  }
  res.end();
}

async function handleSample(req, res, url) {
  const name = url.searchParams.get('name') || '';
  const sample = SAMPLE_CATALOG.find((s) => s.id === name);
  if (!sample) throw new HttpError(404, 'Unknown sample');
  const file = path.join(DATA_DIR, sample.id);
  let content;
  try {
    content = await fsp.readFile(file, 'utf8');
  } catch (_) {
    throw new HttpError(404, 'Sample file is missing from data/');
  }
  res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8', 'Cache-Control': 'public, max-age=300' });
  res.end(content);
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

const server = http.createServer(async (req, res) => {
  const started = Date.now();
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = url.pathname;

  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('Referrer-Policy', 'same-origin');
  res.setHeader('X-Frame-Options', 'DENY');

  try {
    if (pathname === '/api/health' && req.method === 'GET') {
      return sendJson(res, 200, healthPayload());
    }
    if (pathname === '/api/databases' && req.method === 'GET') {
      return sendJson(res, 200, { databases: DATABASE_CATALOG.map(readCatalog) });
    }
    if (pathname === '/api/samples' && req.method === 'GET') {
      return sendJson(res, 200, {
        samples: SAMPLE_CATALOG.map((s) => {
          let bytes = 0;
          try { bytes = fs.statSync(path.join(DATA_DIR, s.id)).size; } catch (_) { /* missing */ }
          return { ...s, bytes, available: bytes > 0 };
        }),
      });
    }
    if (pathname === '/api/sample' && req.method === 'GET') {
      return await handleSample(req, res, url);
    }
    if (pathname === '/api/scan' && req.method === 'POST') {
      return await handleScan(req, res, false);
    }
    if (pathname === '/api/scan/stream' && req.method === 'POST') {
      return await handleScan(req, res, true);
    }
    if (pathname === '/api/version' && req.method === 'GET') {
      return sendJson(res, 200, { version: ENGINE_VERSION, schemaVersion: 2 });
    }
    if (pathname.startsWith('/api/')) {
      return sendJson(res, 404, { error: 'Unknown endpoint' });
    }
    if (req.method !== 'GET' && req.method !== 'HEAD') {
      return sendJson(res, 405, { error: 'Method not allowed' });
    }
    return serveStatic(req, res, pathname);
  } catch (err) {
    const status = err instanceof HttpError ? err.status : 500;
    if (status === 500) log('Request failed:', err);
    if (!res.headersSent) sendJson(res, status, { error: err.message, hint: err.hint });
    else res.end();
  } finally {
    if (pathname.startsWith('/api/')) {
      log(`${req.method} ${pathname} ${res.statusCode} ${Date.now() - started}ms`);
    }
  }
});

server.listen(PORT, HOST, () => {
  const engineReady = fs.existsSync(BINARY);
  console.log('');
  console.log('  GENOME FIREWALL — biosecurity console');
  console.log(`  http://${HOST === '0.0.0.0' ? 'localhost' : HOST}:${PORT}`);
  console.log(`  engine: ${engineReady ? BINARY : 'MISSING — run `make` first'}`);
  console.log(`  catalogs: ${DATABASE_CATALOG.map(readCatalog).map((d) => `${d.id} (${d.markers})`).join(', ')}`);
  console.log('');
});

process.on('SIGINT', () => {
  log('Shutting down');
  server.close(() => process.exit(0));
  setTimeout(() => process.exit(0), 1500).unref();
});
