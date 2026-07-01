---
name: cbmc-encodetree-loop-depth-vacuity
description: EncodeTree (zopfli.c) postcondition is structurally unreachable under --depth 200; kill 0
metadata: 
  node_type: memory
  type: project
  originSessionId: 26ff7348-4127-47d7-9d2f-802b9ec3282a
---

`EncodeTree` in `/app/Syzygy_Zopfli/c_code/zopfli.c` verifies soundly (exit 0)
but mutation kill score is structurally 0, like its siblings [[cbmc-depth-200-object-limit]].

**Why:** `hlit2 = hlit + 257` so `hlit2 >= 257` always, and the main encode loop
runs `for (i = 0; i < lld_total; ...)` with `lld_total = hlit2 + hdist + 1 >= 258`.
This bound is fixed by structure — no input can shorten it. Under the harness's
`--partial-loops --unwind 5 --depth 200`, the loop never exits, so the
`result_size` computation (~line 1199) and the function exit are unreachable.
`__CPROVER_ensures` is only checked at exit → checked vacuously → all 136 mutants
survive (kill 0). The trim loops (`while hlit>0 && ll_lengths[...]`) also only
unwind 5x from hlit=29, so underflow/OOB mutants never manifest either.

**How to apply:** Don't chase kill score on EncodeTree (or similarly large
end-of-body-result functions) — it's capped at 0 by depth. The strongest correct
contract is still worth keeping. Note: `EncodeTree` calls
`ZopfliCalculateBitLengths(clcounts, 19, ...)` but that callee's contract requires
`n <= ZLLCL_MAX_N (8)`; this conflict is masked only because line 1165 is
unreachable within depth 200. Contract used: ll_lengths is_fresh ZOPFLI_NUM_LL,
d_lengths is_fresh ZOPFLI_NUM_D, `out == NULL` (size-only mode), empty assigns,
`return_value >= 14 + 4*3`.
