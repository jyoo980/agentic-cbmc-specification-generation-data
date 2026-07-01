---
name: cbmc-getmatch-param-aliasing-wall
description: GetMatch verifies vacuously (kill 0/40); 3 pointer params must alias one buffer but enforce-contract makes them distinct objects
metadata: 
  node_type: memory
  type: project
  originSessionId: bfac01fc-b09f-4ceb-94ed-0e89373efe75
---

`GetMatch` in `/app/Syzygy_Zopfli/c_code/zopfli.c` walks `scan` and `match` in lockstep over `[scan, end)` with `safe_end == end - 8`. A faithful memory-safety contract needs `end` and `safe_end` to alias `scan`'s buffer (16 and 8 bytes into it).

**The wall:** `goto-instrument --enforce-contract` havocs every pointer parameter into a *distinct* object. So any assumption that aliases two params is unsatisfiable → vacuous:
- `end == scan + 16` (even with no is_fresh at all) → body unreachable (`__CPROVER_assert(0)` at body start passes).
- `__CPROVER_same_object(end, scan) && __CPROVER_pointer_offset(end) == 16` → also vacuous.
- in-bounds variant (scan fresh 17, end = scan+16) → also vacuous.

The only non-aliasing alternative — four disjoint `is_fresh` objects — makes the in-body cross-object `scan < safe_end` comparison emit `same object violation ... FAILURE`, so it does not verify.

**Conclusion:** kept the sound-but-vacuous spec (`is_fresh(scan,16)`, `is_fresh(match,16)`, `end==scan+16`, `safe_end==end-8`, ensures return in `[end-16, end]`). Verifies, kill 0/40, unimprovable in this harness. Note: depth was NOT the blocker here — vacuous even at `--depth 5000`. Generalizes [[cbmc-is-fresh-gotchas]] ("use a second is_fresh instead of aliasing"): you cannot make one havoc'd pointer param point into another's object. Also: half of GetMatch's 40 mutants are on the dead `sizeof(unsigned int)==4` / fallback branches (size_t==8 is true), so unkillable regardless.
