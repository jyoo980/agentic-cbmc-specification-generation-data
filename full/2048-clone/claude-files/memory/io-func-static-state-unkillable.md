---
name: io-func-static-state-unkillable
description: void I/O funcs whose only effects are function-local statics + external calls are unkillable; empty assigns is strongest spec
metadata: 
  node_type: memory
  type: project
  originSessionId: 4d8b3fd9-acc3-4878-a15a-719ad653ebd6
---

For a `void` function with no output pointers whose entire effect is confined to
function-local `static` variables and side-effecting external calls (e.g.
`setBufferedInput` in 2048-clone/2048.c: writes `static bool enabled`/`static
struct termios old`, calls `tcgetattr`/`tcsetattr`), the strongest verifiable
contract is just `__CPROVER_assigns()` (empty) — asserting no caller-visible
memory changes. CBMC does NOT flag function-local static writes against an empty
assigns clause, so it verifies cleanly.

Branch-condition mutants (`&&`->`||` on the `if (enable && !enabled)` guards)
SURVIVE unavoidably: they only change which static/external-only branch runs, and
contracts verify a single call so persistent static state can't be observed. Kill
score 0/2 here is inherent, not a weak spec. Keep the empty-assigns contract.

External `tcgetattr`/`tcsetattr` need stubs in /app/stubs/ (tcgetattr writes
`*termios_p` nondeterministically; tcsetattr is empty-bodied, returns nondet int).
See [[stubs-dir-for-external-callees]].
