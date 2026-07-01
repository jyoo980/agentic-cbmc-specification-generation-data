---
name: patch-distance-codes-survivors
description: PatchDistanceCodesForBuggyDecoders maxes at 8/12 kills; 4 survivors are equivalent or bounded-loop-unkillable
metadata: 
  node_type: memory
  type: project
  originSessionId: 0c429f7b-f5cb-4174-960b-dce48f45ee53
---

PatchDistanceCodesForBuggyDecoders (Syzygy_Zopfli/c_code/zopfli.c) verifies with a
contract of: `__CPROVER_is_fresh(d_lengths, 30*sizeof(unsigned))`, `assigns(d_lengths[0],
d_lengths[1])`, a nested-`__CPROVER_exists` ensures ("≥2 nonzero among the 30 slots"),
and a `__CPROVER_old` ensures ("if old d[0]!=0 && old d[1]!=0 then d[0],d[1] unchanged").

Kill score maxes at **8/12**. The 4 survivors cannot be killed:
- `num_dist_codes > 2` — equivalent. Count increments by 1; delaying early-return to
  count 3 never changes the final array (patch block fires only when post-loop count is 0/1).
- `num_dist_codes == 2` — equivalent. Monotone +1 count crosses through 2, so it returns
  at the same iteration as `>= 2`.
- `i != 30` — equivalent. Runs i=0..29 identically to `i < 30`.
- `i <= 30` — unkillable under the harness. Only effect is reading d_lengths[30] at loop
  iteration 30, which is beyond `--partial-loops --unwind 5`; CBMC truncates the loop long
  before iteration 30, so original and mutant models are identical. is_fresh(30) makes that
  read an OOB kill *if reached*, but the bounded loop never reaches it. Killing needs a
  larger unwind, which is prohibited from being hardcoded. Same bounded-loop ceiling as
  [[avocado-depth200-vacuity]].

Don't chase these 4. Grader: killscore_patch.py in c_code (run with PYTHONSAFEPATH=1 to
dodge the local bisect.py shadowing stdlib). Re-confirmed 2026-06-27: baseline VERIFICATION
SUCCESSFUL via ./verify.sh, still 8/12 with the same 4 survivors.
