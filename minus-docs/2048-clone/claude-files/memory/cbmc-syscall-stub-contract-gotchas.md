---
name: cbmc-syscall-stub-contract-gotchas
description: Gotchas verifying functions that call system-library functions (e.g. tcsetattr/tcgetattr) with in-file CBMC stubs
metadata: 
  node_type: memory
  type: project
  originSessionId: 23b61ba5-cb72-49af-92ad-ba565a0a4947
---

When verifying a C function whose callees are system-library functions declared
in system headers (e.g. `tcgetattr`/`tcsetattr` from `<termios.h>`), `avocado`'s
call-graph classifies them as **external** even if you add in-file stub bodies.
Consequences I confirmed:

- External callees are NOT passed to `--replace-call-with-contract`; their stub
  **bodies are inlined**. So putting `__CPROVER` contracts on the stubs is
  counterproductive — a replaced/used contract decouples the stub's observable
  global side effects (the call gets discharged by contract instead of running
  the body). Give the stubs **plain bodies, no contract**.
- To observe "which branch ran", have each stub bump a file-scope `int` counter,
  `__CPROVER_requires` the counters == 0, and list them in the target's
  `__CPROVER_assigns` (otherwise the inlined write fails the empty default frame:
  `[callee.assigns.N] ... not assignable: FAILURE`).
- Under `--enforce-contract`, a **function-local `static` is treated as
  zero-initialized** (its `= true` initializer is NOT applied) — so a flag
  `static bool enabled = true;` is effectively `false` at entry, making branches
  guarded by it unreachable.
- **Operations on a CBMC-incomplete struct type havoc adjacent global counters.**
  `struct termios` is incomplete to CBMC (`empty*` in the goto); a branch doing
  `old = new;` / `new.c_lflag &= ...;` havocs the global call-counters, so that
  branch's side effects become unobservable at the postcondition. Branches with
  no struct ops stay observable. This blocks killing mutants whose only
  difference is reaching such a struct-manipulating branch.

Diagnosis tactic: `goto-instrument --show-goto-functions <f>.goto` to inspect how
calls/structs compile, and force counterexamples with a deliberately-false
`__CPROVER_ensures` to read entry state via `--trace`. See
[[cbmc-quantifier-isfresh-constant-bound]] for another havoc-in-ensures pitfall.
