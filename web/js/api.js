/* Browser client for the local Genome Firewall API. Same-origin, no CORS. */

const BASE = '';

async function getJSON(path, { signal } = {}) {
  const res = await fetch(`${BASE}${path}`, { headers: { Accept: 'application/json' }, signal });
  const text = await res.text();
  let data = null;
  try { data = text ? JSON.parse(text) : null; } catch (_) { /* not json */ }
  if (!res.ok) {
    const err = new Error(data?.error || `Request failed (${res.status})`);
    err.status = res.status;
    err.hint = data?.hint;
    throw err;
  }
  return data;
}

export function getHealth(options) {
  return getJSON('/api/health', options);
}

export function getDatabases(options) {
  return getJSON('/api/databases', options);
}

export function getSamples(options) {
  return getJSON('/api/samples', options);
}

export async function getSample(name, { signal } = {}) {
  const res = await fetch(`${BASE}/api/sample?name=${encodeURIComponent(name)}`, { signal });
  if (!res.ok) throw new Error('Could not load the sample genome');
  return res.text();
}

/**
 * POST /api/scan/stream and consume NDJSON progress events.
 * onEvent receives {type:'stage'|'log'|'result'|'error', ...}
 */
export async function streamScan(payload, { onEvent, signal } = {}) {
  const res = await fetch(`${BASE}/api/scan/stream`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Accept: 'application/x-ndjson' },
    body: JSON.stringify(payload),
    signal,
  });

  if (!res.ok) {
    let data = null;
    try { data = await res.json(); } catch (_) { /* noop */ }
    const err = new Error(data?.error || `Scan failed (${res.status})`);
    err.status = res.status;
    err.hint = data?.hint;
    err.detail = data?.detail;
    throw err;
  }

  if (!res.body) throw new Error('This browser cannot stream scan progress');

  const reader = res.body.getReader();
  const decoder = new TextDecoder();
  let buffer = '';

  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    buffer += decoder.decode(value, { stream: true });
    let index;
    while ((index = buffer.indexOf('\n')) >= 0) {
      const line = buffer.slice(0, index).trim();
      buffer = buffer.slice(index + 1);
      if (!line) continue;
      let event;
      try {
        event = JSON.parse(line);
      } catch (_) {
        event = { type: 'log', line };
      }
      onEvent?.(event);
    }
  }

  if (buffer.trim()) {
    let event;
    try { event = JSON.parse(buffer); } catch (_) { event = null; }
    if (event) onEvent?.(event);
  }
}

export async function scanOnce(payload, { signal } = {}) {
  const res = await fetch(`${BASE}/api/scan`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
    signal,
  });
  const data = await res.json().catch(() => null);
  if (!res.ok) {
    const err = new Error(data?.error || `Scan failed (${res.status})`);
    err.hint = data?.hint;
    err.detail = data?.detail;
    throw err;
  }
  return data;
}