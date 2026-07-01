---
name: cbmc-initeditor
description: initEditor verifies with full field-pinning ensures; 0 mutants (maxed); needs a signal() stub for __sysv_signal
metadata: 
  node_type: memory
  type: project
  originSessionId: 80af8891-ccfc-49b5-b705-9ffb1b5f2f55
---

initEditor verifies under the canonical run (`--partial-loops --depth 200`,
replace-call-with-contract on updateWindowSize, enforce initEditor). It has **no
mutants** (`get-mutants`: "no mutable operators"), so kill score is 0/0 —
maxed once it verifies. Tiny straight-line function, no depth issues.

Spec: `requires(rk_idx == 0)` (propagates updateWindowSize's precondition, since
its contract is replaced at the call site), `assigns(E, rk_idx)` (whole struct +
rk_idx covers updateWindowSize's E.screenrows/screencols/rk_idx writes), and
ensures pinning every directly-set field to its literal (cx/cy/rowoff/coloff/
numrows/dirty == 0; row/filename/syntax == NULL) plus `E.screencols != 0`
inherited from updateWindowSize's ensures.

KEY GOTCHA: `signal(SIGWINCH, ...)` — glibc with _DEFAULT_SOURCE redirects the
`signal` identifier to `__sysv_signal`, which has no body → canonical run fails
with `no-body.__sysv_signal: FAILURE` (the depth-200 cbmc command has no flag
suppressing no-body). Fix: created `/app/stubs/signal.c` with marker
`/* FUNCTION: signal */` (matches the call graph's `signal` external callee) but
the C function named `__sysv_signal` (the real missing symbol) — a no-op
returning the handler. Stub auto-linked by tools/util/stubs.py only when `signal`
is an external callee (initEditor / handleSigWinCh). Watch comment nesting: don't
write a literal `*/` inside the prose comment or it closes early (PARSING ERROR).

Related: [[cbmc-updatewindowsize]], [[cbmc-getwindowsize]].
