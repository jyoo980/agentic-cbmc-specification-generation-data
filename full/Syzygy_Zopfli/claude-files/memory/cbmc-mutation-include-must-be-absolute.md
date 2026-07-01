---
name: cbmc-mutation-include-must-be-absolute
description: run-cbmc -I paths must be ABSOLUTE or all mutants compile-fail
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 75e9b7ba-5b57-4d17-a93d-e17c834e4a75
---

When invoking `run-cbmc ... -I <dir>`, the include dir MUST be an absolute path
(e.g. `-I /app/Syzygy_Zopfli/stubs`, not `-I ../stubs`).

**Why:** The baseline verification runs goto-cc in the source directory, so a relative
`-I ../stubs` resolves fine and the function "verifies successfully". But mutation testing
runs each mutant's goto-cc with `cwd=<temp scratch dir>` (see `_verify_mutant` in
tools/util/mutation.py). A relative `-I ../stubs` then resolves against /tmp and the stub
headers (e.g. `<x86_64-linux-gnu/bits/types/FILE.h>`) are not found, so EVERY mutant reports
`compile_failed` (goto-cc returncode=1) and the kill score is reported as 0/N with all mutants
compile-failed — a tooling artifact, not real vacuity.

**How to apply:** Always pass absolute `-I` paths. If a run shows ALL mutants compile-failed
(killed=0, compile_failed=N), suspect a relative include path before concluding anything about
the spec. For Syzygy_Zopfli the stubs live at `/app/Syzygy_Zopfli/stubs`.
