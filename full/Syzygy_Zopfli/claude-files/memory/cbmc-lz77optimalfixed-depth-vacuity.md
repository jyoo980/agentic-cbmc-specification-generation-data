---
name: cbmc-lz77optimalfixed-depth-vacuity
description: "ZopfliLZ77OptimalFixed verifies soundly but kill 0/3; malloc-size mutants unreachable behind LZ77OptimalRun's 65536-elem hash is_fresh at depth 200"
metadata: 
  node_type: memory
  type: project
  originSessionId: 2b5d5116-6e40-4eda-9dc3-a46523d1c4e6
---

`ZopfliLZ77OptimalFixed` (Syzygy_Zopfli/c_code/zopfli.c ~4450) verifies with a
full memory-safety contract + functional postcondition (s->blockstart==old(instart),
s->blockend==old(inend)) but **kill 0/3**.

**Why:** Despite in-body malloc (length_array, costs) and in-body free, goto-instrument did NOT crash here — contradicting the in-body-malloc/free crash notes ([[cbmc-inbody-malloc-enforcement-crash]], [[cbmc-inbody-free-deallocate-crash]]). It verified fine. (Those crashes are not universal; this pure-wrapper shape passes.)

All 3 surviving mutants are the local sizing arithmetic: `blocksize = inend - instart` (`-`→`+`) and the two `malloc(... * (blocksize + 1))` (`+`→`-`). These buffers are never read in the function — they're handed to `LZ77OptimalRun` (contract REPLACED). The only place a wrong size could be caught is the call-site `is_fresh` assertions for LZ77OptimalRun's preconditions, but that list includes `is_fresh(h->head, 65536*sizeof(int))` and `is_fresh(h->head2, 65536*sizeof(int))` — checking is_fresh on 256 KB objects exhausts `--depth 200` before the length_array/costs checks run. Same depth-200/65536-hash wall as [[cbmc-zopflifindlongestmatch-depth-vacuity]], [[cbmc-zopfliupdatehash-depth-vacuity]].

The blockstart/blockend assignments precede the LZ77OptimalRun call so that postcondition IS live/sound, but it's independent of blocksize so can't kill the malloc mutants. Ceiling = 0/3.

**How to apply:** Don't chase these malloc-size mutants — pinning blocksize=1 (instart+1==inend, inend<=ZMCS_MAXPOS) and the full callee is_fresh set is required for LZ77OptimalRun's replaced contract; the 65536-elem hash is_fresh is unavoidable and is the wall. Build needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` (FILE.h stub).
