---
name: zalz-weakstub-4of5
description: "ZopfliAppendLZ77Store 4/5 kills via weak-stub callee override + no depth bound; beats loop-contract depth-200 vacuity, survivor is equivalent != mutant"
metadata: 
  node_type: memory
  type: project
  originSessionId: 72703ba5-6864-42f1-b4e9-b2163c4be48c
---

ZopfliAppendLZ77Store (zopfli.c) — copy loop `for(i=0;i<store->size;i++)
ZopfliStoreLitLenDist(...,target)`. All 5 mutants are loop-guard mutants.
Achieved **4/5 kills** (max legitimate); survivor `i != store->size` is a true
EQUIVALENT mutant (counter +1 from 0 hits `==size` exactly ⇒ `!=`≡`<`), a sound
spec MUST leave it verifying — don't chase it.

**Two obstacles, both overcome:**

1. *Loop-contract depth-200 vacuity.* Enforce-contract + apply-loop-contracts +
   --depth 200/400/800 → original verifies but 0/5: the dfcc prologue pushes the
   loop body past the depth frontier, AND the loop-contract exit `assume(!guard)`
   combined with invariant `target->size==loop_entry+i` makes the post-state
   INFEASIBLE for zero-iteration mutants (i<size contradicts i==size), so both
   the postcondition and a post-loop `assert(i==store->size)` pass VACUOUSLY.
   FIX: run with **NO --depth**. After apply-loop-contracts + replace-call, there
   are no unbounded loops left, so dropping --depth is sound and reaches every
   check. Then the post-loop assert kills the 3 zero-iter mutants (i>=,==,>) and
   OOB read of store->litlens[size] (source arrays is_fresh-sized to EXACTLY
   store->size) kills `i<=size`.

2. *Callee contract is loop-incompatible.* ZopfliStoreLitLenDist's real contract
   ([[zslld-depth200-vacuity]]) requires is_fresh(store) + non-power-of-two size
   + fresh arrays sized size+1 — these CANNOT be re-established in a loop
   invariant (is_fresh isn't a state predicate; size changes each iter), so
   replace-call-with-contract on the real callee FAILS the original at no-depth.
   FIX: a **weak sound stub** in /app/stubs/zslld_weak.c — same signature, body
   `target->size++`, assigns target->size + object_whole of all 8 buffers,
   ensures size==old+1. Over-approximates buffer writes, exactly captures the
   size bump (all the caller's spec depends on). Sound: caller never reads buffer
   contents/pointers; size==old+store->size holds in reality too.

**Plumbing to override the strong contract (it lives in same zopfli.c):**
goto-cc caller TU (`--function __zalz_harness`, with harness) →
`goto-instrument --remove-function-body ZopfliStoreLitLenDist` (drops body, NOT
contract) → `goto-cc weak.goto base.goto --function __zalz_harness` with **weak
FIRST** so its contract wins the merge (strong-first ⇒ strong contract used ⇒
fails) → dfcc --enforce-contract caller --replace-call-with-contract callee
--apply-loop-contracts → cbmc NO --depth. Must set `--function __zalz_harness`
on the LINK step too or entry resets and dfcc instruments the whole program
(crashes on nested-loop-without-contract). Do NOT --add-library (pulls in main).

Scripts: /app/_verify_zalz.sh, /app/kill_zalz.sh, /app/_zalz_harness.c,
/app/stubs/zslld_weak.c. ZopfliStoreLitLenDist's own contract untouched, still
self-verifies. Loop assigns must be ONE __CPROVER_assigns(comma,list), not
multiple clauses (syntax error otherwise).
