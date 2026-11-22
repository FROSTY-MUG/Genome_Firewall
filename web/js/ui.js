/* Small DOM / formatting helpers shared by every view. */

import { icon } from './icons.js';

export { icon };

export function esc(value) {
  return String(value ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

export function fmtInt(n) {
  const num = Number(n);
  if (!Number.isFinite(num)) return '—';
  return Math.round(num).toLocaleString('en-US');
}

export function fmtBytes(bytes) {
  const n = Number(bytes);
  if (!Number.isFinite(n) || n < 0) return '—';
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(n < 10240 ? 1 : 0)} KB`;
  return `${(n / 1024 / 1024).toFixed(1)} MB`;
}

export function fmtMs(ms) {
  const n = Number(ms);
  if (!Number.isFinite(n)) return '—';
  if (n >= 1000) return `${(n / 1000).toFixed(2)} s`;
  if (n >= 100) return `${n.toFixed(0)} ms`;
  if (n >= 10) return `${n.toFixed(1)} ms`;
  return `${n.toFixed(2)} ms`;
}

export function pct(fraction, digits = 1) {
  const n = Number(fraction);
  if (!Number.isFinite(n)) return '—';
  return `${(n * 100).toFixed(digits)}%`;
}

export function statusMeta(status) {
  switch (status) {
    case 'LIKELY TO FAIL':
      return { key: 'fail', label: 'Resistant', full: 'Likely to fail', cls: 'pill--fail' };
    case 'LIKELY TO WORK':
      return { key: 'work', label: 'Susceptible', full: 'Likely to work', cls: 'pill--work' };
    default:
      return { key: 'warn', label: 'No-call', full: 'No-call', cls: 'pill--warn' };
  }
}

export function tierMeta(tier) {
  if (String(tier).includes('III')) {
    return { key: 3, label: 'Tier III', title: 'Molecular target intact', color: 'var(--mint)' };
  }
  if (String(tier).includes('II')) {
    return { key: 2, label: 'Tier II', title: 'Statistical association', color: 'var(--amber)' };
  }
  return { key: 1, label: 'Tier I', title: 'Direct marker match', color: 'var(--red)' };
}

export function debounce(fn, ms = 160) {
  let handle;
  return (...args) => {
    clearTimeout(handle);
    handle = setTimeout(() => fn(...args), ms);
  };
}

export function toast(message, { tone = 'ok', title = '', timeout = 4200 } = {}) {
  const host = document.getElementById('toasts');
  if (!host) return;
  const node = document.createElement('div');
  node.className = `toast${tone === 'err' ? ' toast--err' : ''}`;
  node.innerHTML = `${icon(tone === 'err' ? 'alert' : 'check')}<div>${title ? `<b>${esc(title)}</b><br>` : ''}${esc(message)}</div>`;
  host.appendChild(node);
  const remove = () => {
    node.classList.add('is-leaving');
    setTimeout(() => node.remove(), 240);
  };
  setTimeout(remove, timeout);
  node.addEventListener('click', remove);
}

export async function copyText(text) {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch (_) {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.setAttribute('readonly', '');
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.select();
    let ok = false;
    try { ok = document.execCommand('copy'); } catch (_) { /* noop */ }
    ta.remove();
    return ok;
  }
}

export function download(filename, text, mime = 'text/plain') {
  const blob = new Blob([text], { type: `${mime};charset=utf-8` });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.rel = 'noopener';
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1500);
}

export function timeAgo(iso) {
  if (!iso) return '—';
  const then = new Date(iso).getTime();
  if (!Number.isFinite(then)) return '—';
  const diff = Math.max(0, Date.now() - then);
  const mins = Math.floor(diff / 60000);
  if (mins < 1) return 'just now';
  if (mins < 60) return `${mins}m ago`;
  const hours = Math.floor(mins / 60);
  if (hours < 24) return `${hours}h ago`;
  return new Date(iso).toLocaleDateString();
}

export function parseFastaStats(content) {
  const text = String(content || '');
  const bytes = new Blob([text]).size;
  let bases = 0;
  let gc = 0;
  let ambiguous = 0;
  let headers = 0;
  const composition = { A: 0, C: 0, G: 0, T: 0 };
  for (const line of text.split('\n')) {
    if (line.startsWith('>')) { headers++; continue; }
    for (const ch of line) {
      const c = ch.toUpperCase();
      if (c === 'A' || c === 'C' || c === 'G' || c === 'T') {
        bases++;
        composition[c]++;
        if (c === 'C' || c === 'G') gc++;
      } else if (c === 'N') {
        ambiguous++;
      }
    }
  }
  return {
    bytes,
    bases,
    headers,
    ambiguous,
    gcPercent: bases ? Number(((gc / bases) * 100).toFixed(2)) : 0,
    composition,
    valid: bases >= 21,
  };
}

export function prefersReducedMotion() {
  return window.matchMedia('(prefers-reduced-motion: reduce)').matches;
}

export function scrollToTop() {
  window.scrollTo({ top: 0, behavior: prefersReducedMotion() ? 'auto' : 'smooth' });
}

export function focusMain() {
  const main = document.getElementById('main');
  if (main) {
    main.focus({ preventScroll: true });
    window.scrollTo(0, 0);
  }
}