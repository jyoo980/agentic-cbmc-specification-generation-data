---
name: tracebackwards-depth200-vacuity
description: "TraceBackwards verifies w/ strong forall-precondition contract but 1/23 kills @depth200; all 22 loop/mirror mutants killable only @depth>=300, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: e6cfeda4-7e96-4bb6-9d9f-64fb820fc1d7
---

TraceBackwards (zopfli.c) — backward LZ77 path trace: unbounded `for(;;)` loop
appending length_array steps via ZOPFLI_APPEND_DATA, then a mirror/reverse loop.

**Contract that verifies @depth200** (enforce-contract, --partial-loops --unwind 5):
- `is_fresh(length_array, (size+1)*sizeof)` — walk reads index `size` down to 1, so size+1 elems (GetBestLengths writes length_array[0..blocksize]).
- `is_fresh(path)`, `is_fresh(pathsize)`, `requires(*pathsize == 0)` (caller LZ77OptimalRun resets path/pathsize to 0 before call).
- `requires(size <= ZOPFLI_WINDOW_SIZE)` — needed: unconstrained size_t makes `(size+1)*sizeof` overflow → is_fresh region 0 bytes → forall reads OOB. Domain-faithful (block fits window) AND prevents overflow.
- `forall j in [1,size]: length_array[j]>=1 && <=j && <=ZOPFLI_MAX_MATCH` — lifts the 3 in-loop asserts to a precondition so the backward walk can rely on them. Quantified REQUIRES work fine here (unlike quantified loop invariants).
- `assigns(*path, *pathsize)` — writes to (*path)[i] go to malloc'd-inside-fn memory, always allowed.
- ensures `size!=0 ==> *pathsize>=1`, `size==0 ==> *pathsize==0`.

**Kill score: 1/23 @depth200, 23/23 @depth300+** (frontier between 250 and 300).
- The 1 kill @200 is the `if(size==0)→!=0` mutant, caught by the ensures on the SHALLOW early-return path (no loop needed).
- The other 22 (5 `<=index` cmp, 5 `<=MAX_MATCH` cmp, `!=0`, `if(index==0)`, 6 mirror-loop-cond, 4 mirror-index-arith) all require executing >=1 loop iteration. Each in-loop assert sits AFTER a full ZOPFLI_APPEND_DATA (malloc+memset+write) → ~270 path depth, ~70 past the depth-200 budget. Intrinsic path cost, NOT prologue cost.

**r_ok/w_ok lever FAILS here**: swapping is_fresh→r_ok(length_array)+w_ok(path,pathsize) still verifies but gives 0/23 @200 (loses even the postcondition kill — w_ok doesn't allocate, makes ensures vacuous). is_fresh is strictly better. Lever helps only when prologue dominates; here loop-body append dominates, so nothing recovers kills @200.

Same family as [[zslld-depth200-vacuity]], [[zuh-depth200-vacuity]], [[silmc-depth200-vacuity]]. Spec is strong & sound; don't chase the 22.

Scripts: /app/_verify_tb.sh, /app/kill_tb.sh (SRC/DEPTH/UNWIND overridable).
