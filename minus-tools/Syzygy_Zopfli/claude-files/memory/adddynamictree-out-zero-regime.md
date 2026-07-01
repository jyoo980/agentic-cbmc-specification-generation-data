---
name: adddynamictree-out-zero-regime
description: AddDynamicTree in zopfli.c must be specified with out==0 and scores 0 mutant kills inherently
metadata: 
  node_type: memory
  type: project
  originSessionId: 40c335db-f395-49e9-93fe-4a393a01fc17
---

`AddDynamicTree` (Syzygy_Zopfli/c_code/zopfli.c) calls `EncodeTree` 9× — 8 size-only
probe calls in a loop to pick the cheapest `use_16/17/18` combo (`best`), then one
final emitting call `EncodeTree(..., best&1, best&2, best&4, bp, out, outsize)`.

`EncodeTree`'s verified contract is the **size-only** spec: it `requires(out == 0)`
and `assigns()` nothing. Under the avocado pipeline every in-file callee is
`--replace-call-with-contract`, so the final call's `out` arg must satisfy
`out == 0`. The only way to discharge that is `__CPROVER_requires(out == 0)` on
AddDynamicTree itself. Verifying contract:
```
requires is_fresh(ll_lengths, 288*sizeof) , is_fresh(d_lengths, 32*sizeof)
requires forall kl<288 => ll_lengths[kl] <= 15 ; forall kd<32 => d_lengths[kd] <= 15
requires out == 0
assigns()
```
This VERIFIES (run `sh verify-rec.sh AddDynamicTree zopfli.c EncodeTree`).

**Kill score is inherently 0** (all 12 mutants on lines `for i<8` and the
`size < bestsize` selection survive — see test-mutants-adt.sh). Reason: with
out==0 the function is a no-op, and even with out!=0 the contract abstracts
EncodeTree's size as nondeterministic and untied to the flags, so `best` is never
observable in any postcondition. Not a weak spec — a methodology limit, same
family as [[avocado-depth200-vacuity]].

**Why:** selection/argmin logic over a contract-replaced callee can't be checked
without inlining the callee's body, which the pipeline never does.
**How to apply:** don't burn the 5-attempt budget chasing kills here; the verifying
out==0 contract is the strongest achievable.
