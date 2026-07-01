---
name: cbmc-enablerawmode
description: enableRawMode verifies; spec is 6/6-strong but canonical depth-200 caps kills at 1/6 (global-init eats budget before the deep tcsetattr branch)
metadata: 
  node_type: memory
  type: project
  originSessionId: 85242b10-2fab-4718-8e5c-e50f5e00884e
---

`enableRawMode(int fd)` in kilo.c verifies and the spec is maximally strong
(kills all 6 mutants at --depth >= 225) but the canonical --depth 200 run scores
only **1/6**.

Mutants (6): five comparison flips on `tcsetattr(...) < 0` (line 242: `<=0 >0
>=0 ==0 !=0`) and one on `tcgetattr(...) == -1` (line 224: `!= -1`). All compare
a fully nondet syscall return, so they can only be killed by modeling the syscall
results.

**Approach (works):** new stub `stubs/rawmode.c` models the 4 external callees
(`isatty`/`tcgetattr`/`tcsetattr`/`atexit`) so each returns a global
(`rm_isatty`/`rm_tcget`/`rm_tcset`); also models `__errno_location` to return a
stable global `rm_errno` (the fatal path writes `errno = ENOTTY`, and CBMC's
no-body `__errno_location` returns a fresh nondet pointer each call that neither
assigns nor ensures can pin — `errno`/`__CPROVER_assigns(errno)` itself is
rejected with "side-effects not allowed in assigns clause targets"). Stub is
auto-linked only for enableRawMode/disableRawMode (no regression to verified set).
Contract: `requires(E.rawmode==0)`, `assigns(E.rawmode, rm_errno)`, and the
killer ensures `(__CPROVER_return_value==0) == (rm_isatty!=0 && rm_tcget!=-1 &&
rm_tcset>=0)` plus `(E.rawmode==1)==(return==0)` and `return==-1 ==> rm_errno==ENOTTY`.

**Why canonical 1/6:** structural depth truncation (same pattern as
[[cbmc-editorreadkey]]). Global nondet-init (HLDB/C_HL_keywords/E) consumes
~175 of the 200 steps before the function body runs. The tcgetattr check is
shallow → its mutant dies. The tcsetattr check sits *after* the 6 fixed
`raw.c_*flag` struct assignments, reached only at total depth ~225, so at
--depth 200 that path is truncated → postconditions vacuous → 5 tcsetattr
mutants survive. Crossover measured exactly: 1/6 at depth<=220, 6/6 at depth>=225.
Cannot fix via spec (C code fixed; global-init cost structural; depth is a fixed
CLI arg). Scorer: `kilo/run_mutants_enableRawMode.py`.
