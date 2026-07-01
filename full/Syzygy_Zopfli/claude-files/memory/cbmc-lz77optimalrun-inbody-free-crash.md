---
name: cbmc-lz77optimalrun-inbody-free-crash
description: LZ77OptimalRun undischargeable; in-body free(*path) crashes __CPROVER_deallocate at goto-instrument
metadata: 
  node_type: memory
  type: project
  originSessionId: 6f4d51b0-f42d-4916-baca-bd8e9ab1a402
---

`LZ77OptimalRun` in `/app/Syzygy_Zopfli/c_code/zopfli.c` is UNDISCHARGEABLE. Its
body calls `free(*path)`; during *Enforcing contracts* goto-instrument aborts with
an invariant violation in builtin `__CPROVER_deallocate`
(`return_value___VERIFIER_nondet___CPROVER_bool`, the `--malloc-may-fail` nondet
free model) — the wall from [[cbmc-inbody-free-deallocate-crash]] /
[[cbmc-inbody-malloc-enforcement-crash]]. Crash reproduces WITH or WITHOUT the
`__CPROVER_frees(*path)` clause, so it is the body `free`, not the spec.

**Why CleanLZ77Store/CleanBlockState tolerated in-body free but this doesn't:**
unclear, but those are pure free-dispatch; this one frees AND replaces heavy callee
contracts (GetBestLengths/TraceBackwards/FollowPath) — the deallocate model trips
here.

Two further walls behind it even if the free wall were lifted: (1) passes havoc'd
`CostModelFun *costmodel` to GetBestLengths → `costmodel$object was not found`
funptr-removal crash ([[cbmc-getbestlengths-funptr-crash]]); (2) replaced callee
preconditions are mutually inconsistent — GetBestLengths requires `inend <=
ZMCS_MAXPOS` (==4) while FollowPath requires `inend == 8` — plus depth-200 wall from
the huge is_fresh set.

**How to apply:** left the strong memory-safety contract (full s/lmc + 9 hash
fields + store + DP arrays is_fresh, blocksize==1 pins, `frees(*path)`, return
`>=0 && < ZOPFLI_LARGE_FLOAT`) documenting intent. Needs `-I
/app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` for FILE.h.
