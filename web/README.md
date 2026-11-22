# Genome Firewall — web console

A local, dependency-free UI for the `genome_firewall` engine. It is plain
HTML/CSS/ES modules served straight from disk: no bundler, no `npm install`,
no external font or icon CDN. That keeps it usable offline on a laptop, in a
container, and inside Termux on Android.

```
web/
├── index.html          app shell, footer, command palette, mobile tab bar
├── favicon.svg
├── css/app.css         design tokens + components + responsive rules
└── js/
    ├── app.js          hash router, engine chip, command palette, boot
    ├── api.js          API client, NDJSON progress reader
    ├── ui.js           formatting, FASTA inspection, toasts, export helpers
    ├── icons.js        inline 24×24 icon set
    └── views/
        ├── overview.js landing page
        ├── scanner.js  upload → configure → live run
        └── report.js   verdict, filterable table, exports
```

## Run

```bash
make                 # build the C++ engine
node api/server.js   # http://127.0.0.1:8787
```

You can also serve `web/` from any static server, but scanning needs the API
on the same origin (paths are relative).

## Screen flow

1. **Overview** — what the engine does, how the five-stage pipeline fits
   together, live catalogue counts from `/api/health`, and the evidence tiers.
2. **Analyse** — drag/drop or paste FASTA (or load a bundled demo). Pick a
   catalogue, SNP tolerance, threading and the confidence gate. While the scan
   runs, the right rail shows real stage transitions and the engine's own log
   lines, streamed as NDJSON.
3. **Report** — verdict banner, resistance/no-call/susceptible counts, run
   telemetry, a sortable and filterable table, and expandable rows that explain
   the evidence behind each call. Export JSON, CSV, or a text summary.

`Ctrl/⌘ K` opens the command palette (navigation, demo loads, exports).
Settings and the last report are stored in `localStorage`.

## Design notes

- **Instrument, not dashboard.** Near-black surfaces, hairline borders, mint
  as the single signal colour, amber/red reserved for no-call/resistant.
- **Data is monospaced, prose is not.** Numbers, IDs and catalogue names use a
  tabular mono stack so columns line up without a webfont.
- **Status is never colour-only.** Pills carry text (`Resistant`, `No-call`,
  `Susceptible`) and the table exposes probability numerically.
- **Mobile is a first-class target.** Bottom tab bar under 640 px, stacked
  workbench, card-style result rows with per-cell labels.
- **No lying UI.** Progress comes from the engine's real stderr, not a timer.
  If the API is down the header says `engine offline`; if the binary is missing
  it says `binary missing`.

## API contract

`POST /api/scan/stream` returns newline-delimited JSON:

```json
{"type":"stage","id":"index","status":"active"}
{"type":"log","line":"[+] Successfully ingested 455 markers ..."}
{"type":"stage","id":"index","status":"done","detail":"352.140 ms"}
{"type":"result","data":{ "schemaVersion":2, "predictions":[...] }}
{"type":"error","message":"...","detail":"..."}
```

Stages: `validate → index → scan → infer → report`. The `result` object is the
engine's `--json` payload plus `request` metadata added by the server.

## Accessibility & performance

- Keyboard: skip link, focus-visible rings, native `<dialog>` palette,
  sortable/filterable rows operable with Enter/Space, `aria-live` toasts.
- Reduced-motion media query disables the scan animation, spinners and reveals.
- No webfonts, no images, no framework runtime; the whole UI is ~70 KB of
  source and renders on the first paint after a single health request.

## Extending

Views are factories: `renderX(state, ctx) -> HTMLElement`, optionally returning
a `__cleanup` function on the element. `ctx` exposes `navigate`,
`saveSettings`, `setLastResult` and `consumePendingSample`. Add a route in
`app.js` (`ROUTES`) and a view module; the header, nav highlighting and command
palette are already generic.
