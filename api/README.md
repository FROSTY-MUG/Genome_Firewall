# Genome Firewall — local API

A single-file Node server that hosts the web console and exposes the C++ engine
over JSON. It deliberately has **no npm dependencies**, so `node api/server.js`
is the entire install step.

```bash
make                 # build ../genome_firewall
node api/server.js   # http://127.0.0.1:8787
```

## Environment

| Variable | Default | Meaning |
| :--- | :--- | :--- |
| `PORT` | `8787` | HTTP port |
| `HOST` | `127.0.0.1` | Bind address (`0.0.0.0` for LAN access) |
| `MAX_UPLOAD_MB` | `40` | Maximum FASTA payload |
| `SCAN_TIMEOUT_MS` | `180000` | Kill the engine after this long |

## Endpoints

- `GET /api/health` — engine/binary status, host CPU+memory, catalogue and sample metadata.
- `GET /api/databases` — whitelisted catalogues with live marker/class counts.
- `GET /api/samples`, `GET /api/sample?name=…` — bundled demo genomes.
- `POST /api/scan` — blocking scan; returns the engine's JSON report.
- `POST /api/scan/stream` — same scan as newline-delimited JSON progress:

```
{"type":"stage","id":"index","status":"active"}
{"type":"log","line":"[+] Successfully ingested 455 markers ..."}
{"type":"result","data":{...}}
```

## Guardrails

- Catalogue IDs are checked against an in-process whitelist; the browser cannot
  point the engine at arbitrary paths.
- Uploads are written to `os.tmpdir()` with a random name, mode `0600`, and
  deleted when the child process closes (including on error/timeout).
- The child is killed on client disconnect, timeout, or cancel.
- The engine binary writes diagnostics to `stderr`; `stdout` is parsed as JSON,
  so log lines can never corrupt the report.
- `path.resolve` + prefix check prevents static path traversal.
- `X-Content-Type-Options`, `X-Frame-Options: DENY`, `Referrer-Policy` are set
  on every response.

## Why no Express?

The previous iteration used Express. For a single-user local tool the Node
standard library is enough and starts instantly on Termux/Android, with no
`node_modules` to reinstall after a fresh clone.
