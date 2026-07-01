---
name: stubs-dir-for-external-callees
description: "External callees with no CBMC model are stubbed via /app/stubs/*.c with a /* FUNCTION: name */ marker"
metadata: 
  node_type: memory
  type: project
  originSessionId: e017aad1-097e-407d-b760-5e3e72aefb46
---

When a function calls an external library function that CBMC has no body for (e.g. `srand`), `run-cbmc` reports `no-body.<name>: FAILURE`. Fix by adding a stub `.c` file under `/app/stubs/` (the dir may not exist — create it). Each stub must precede its definition with a `/* FUNCTION: <name> */` comment marker; `tools/util/stubs.py::build_stub_index` indexes by that marker and the runner auto-compiles the matching stub alongside the source. CBMC already models `rand`, `time`, `malloc` via its builtin library, so those need no stub — only genuinely unmodeled callees do.

Example created for this repo: `/app/stubs/srand.c` — empty-body `void srand(unsigned)`, sound since `rand` is already nondet. Related: [[cbmc-depth-200-vacuous-loops]].
