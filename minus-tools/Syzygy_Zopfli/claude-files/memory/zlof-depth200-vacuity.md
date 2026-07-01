---
name: zlof-depth200-vacuity
description: ZopfliLZ77OptimalFixed verifies @depth200 but 0/3 kills (vacuity); spec proven strong (3/3 @depth800) via OBJECT_SIZE callee preconditions
metadata: 
  node_type: memory
  type: project
  originSessionId: dbdb1e48-314c-4b33-aa02-21489730bba0
---

ZopfliLZ77OptimalFixed (zopfli.c ~4199) verifies under the official harness
(`--depth 200`, PASS) but scores **0/3 @depth200** — a [[avocado-depth200-vacuity]]
case. All 3 mutants are on the prologue (length_array malloc `blocksize+1`→`-1`,
costs malloc `blocksize+1`→`-1`, `blocksize = inend-instart`→`inend+instart`).

**The spec IS strong — proven 3/3 killable @depth800, just vacuous @200.** Lever:
the only place the allocation sizes are *checked* is the `LZ77OptimalRun(...)` call
site (callee replaced by contract), and that call sits past the depth-200 frontier
(prologue cost = 2 mallocs + ZopfliAllocHash's 7 is_fresh ensures + ZopfliCleanHash
frame). At depth 400 still vacuous; at depth 800 the `LZ77OptimalRun` precondition
assertions become reachable and flip: MUT-LA→precondition (length_array OBJECT_SIZE)
FAILURE, MUT-COSTS→costs OBJECT_SIZE FAILURE, MUT-BS→both FAILURE; ORIG all SUCCESS.

**Key technique (reusable):** to kill a *caller's* malloc-size mutant when the
callee is contract-replaced, put the size requirement in the **callee's `requires`
as a plain boolean** `__CPROVER_OBJECT_SIZE(buf) == sizeof(T)*(n)`, NOT as
`__CPROVER_is_fresh(buf, sizeof(T)*n)`. Under this legacy `--replace-call-with-contract`
pipeline, **is_fresh in a callee requires is NOT asserted as a size check at the call
site** (under-alloc mutant survived even @depth800 with is_fresh); a plain OBJECT_SIZE
boolean predicate IS asserted and kills the mutants. Confirmed empirically.

Edits made (both sound, both kept):
- LZ77OptimalRun: added `requires(OBJECT_SIZE(length_array)==sizeof(short)*(inend-instart+1))`
  and same for `costs` (genuine GetBestLengths requirement). LZ77OptimalRun stays
  UNVERIFIABLE — [[lor-fptr-param-crash]] rc134, unchanged by the added requires.
- ZopfliLZ77OptimalFixed: `requires(is_fresh(s))`, `requires(instart<=inend)`,
  `requires(inend-instart < max_malloc_size/sizeof(float)-1)`, `assigns(s->blockstart,
  s->blockend)`, `ensures` exact-value on both. (Without these the deep proof fails on
  s-deref + unforwarded instart<=inend; @200 they're sound assumptions.)

Other caller ZopfliLZ77Optimal has NO contract (rc6 @goto-instrument), pre-existing,
unaffected. Don't chase the @200 kills — pure vacuity. Scripts: /app/_verify_zlof.py
(official), /app/_score_zlof.py (3 mutants), /app/_verify_zlof.sh (manual, adjustable
depth), /app/_regress_check.py.
