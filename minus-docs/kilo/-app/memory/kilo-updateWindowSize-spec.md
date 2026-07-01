---
name: kilo-updatewindowsize-spec
description: "updateWindowSize verifies NON-vacuously 1/1 kill, but only after switching getWindowSize+getCursorPosition reqs from is_fresh to w_ok (passes &E.screenrows globals)."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5d2f3e48-b7d6-4c98-a554-52165c9ffae9
---

`updateWindowSize` (kilo.c, no params, calls [[kilo-getWindowSize-spec]] with
`&E.screenrows`/`&E.screencols`) verifies with `__CPROVER_assigns(E.screenrows, E.screencols)`
and a **1/1 (100%) kill score** — non-vacuous (confirmed: a false `ensures(E.screencols==12345)`
probe FAILS). The lone mutant (`E.screenrows -= 2` → `+= 2`) is killed via the signed-overflow
check: original `-2` proves no-overflow, mutant `+2` overflows.

**Key blocker fixed:** getWindowSize's contract originally required `is_fresh(rows/cols)`. As a
*replaced callee* that requires is ASSERTED at updateWindowSize's call site, and is_fresh is FALSE
for global subobjects `&E.screenrows`/`&E.screencols` → genuine `precondition: FAILURE` (NOT the
usual vacuous UNSAT poison). Fix: switch BOTH getWindowSize AND getCursorPosition preconditions
from `__CPROVER_is_fresh` to `__CPROVER_w_ok(p, sizeof(int))`, which globals satisfy. Cascade is
contained (callgraph: getCursorPosition←getWindowSize←updateWindowSize, no other callers).

**Dead end (don't retry):** a variadic `sscanf` stub to bound the parsed dims CRASHES
`goto-instrument --enforce-contract` (the documented variadic crash). So getCursorPosition can't
bound its output. Not needed anyway — updateWindowSize verifies without it.
