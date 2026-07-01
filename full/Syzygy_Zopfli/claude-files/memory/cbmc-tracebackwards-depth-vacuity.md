---
name: cbmc-tracebackwards-depth-vacuity
description: TraceBackwards verifies soundly but kill 0/23; ZOPFLI_APPEND_DATA macro + 4 is_fresh exhaust depth 200 before exit
metadata: 
  node_type: memory
  type: project
  originSessionId: 3436e832-5eae-4ad3-bf0c-1cd6d655bc64
---

`TraceBackwards` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies soundly but kill 0/23.

**Contract approach (sound):** pin `size == 1`, `is_fresh(length_array, (size+1)*2)` with
`length_array[size] == size` (terminal hop so the data-dependent `for(;;)` reaches index 0 in
one append and the 3 in-body asserts hold), `is_fresh(path)` + `is_fresh(*path, 4*2)`,
`is_fresh(pathsize)` + `*pathsize == 3` (non-power-of-two so ZOPFLI_APPEND_DATA writes in
place — no realloc). assigns `*pathsize, object_whole(*path)`; ensures
`*pathsize == old+1` and `(*path)[0] == size`.

**Why kill 0:** depth-vacuity wall. Injected `ensures(1 == 0)` PASSES → function exit
unreachable under fixed `--depth 200`. The 4 is_fresh objects plus the ZOPFLI_APPEND_DATA
malloc/realloc/memset goto-machinery (present even though the branch is concretely not taken
at `*pathsize==3`) exhaust the 200-step budget before exit. All 23 survivors are arithmetic
mutants on the trace loop (3906-3910) and mirror loop (3940-3944); even OOB-producing mirror
mutants survive, confirming the mirror loop body is never checked.

Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h. Same family as [[cbmc-storelitlendist-depth-vacuity]],
[[cbmc-storeinlongestmatchcache-depth-vacuity]] — depth fixed by harness, unwinnable.
The malloc in the macro did NOT crash goto-instrument here (unlike [[cbmc-inbody-malloc-enforcement-crash]]),
matching AddBit which also uses the macro with the *outsize==3 pin.
