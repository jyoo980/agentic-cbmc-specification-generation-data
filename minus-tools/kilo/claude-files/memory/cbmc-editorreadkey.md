---
name: cbmc-editorreadkey
description: "editorReadKey verifies but canonical depth-200 caps kills at 1/21; spec is actually 18/21-strong, 3 survivors are requires-equivalent"
metadata: 
  node_type: memory
  type: project
  originSessionId: ef8f6b3c-21b9-4da0-bdc9-92484862b636
---

editorReadKey (kilo.c) verifies under the canonical pipeline (goto-cc + /app/stubs/readkey.c, --partial-loops --unwind 5, --enforce-contract, --depth 200) → VERIFICATION SUCCESSFUL. Stub `/app/stubs/readkey.c` models read() via scripted rk_idx/rk_ret[6]/rk_byte[6] globals and exit() as `__CPROVER_assume(0)`. Contract pins return value exactly as a function of the scripted bytes, with a restrictive `requires` that admits only first-pass-of-the-decode-loop inputs (loop-again cases out of scope).

Canonical kill score is **1/21** — ONLY mutant #17 (`nread==-1`→`!=-1`, the exit guard) dies, because its violating path is short. Every escape-decode mutant needs a counterexample path reaching the decode returns at **~depth 260**, but **~150 of the 200 budget is eaten by global initialization** of kilo.c's syntax-highlight tables (esp. `C_HL_keywords[]`, ~90 string-pointer assignments). Measured thresholds: plain-key postcondition reachable ~180; mut16 (seq[0] read) flips survive→kill exactly at depth 260. So escape postconditions are VACUOUS at 200 (see also [[cbmc-depth-vacuous-and-recursive-callees]]).

The spec is NOT weak: at --depth 3000 it kills **18/21**. The 3 permanent survivors (mut4 `<='9'`→`<'9'`; mut11 `>='0'`→`>'0'`; mut13 `>='0'`→`!='0'`) differ only at seq[1]=='0'/'9', which are out-of-scope loop-again inputs the requires excludes → genuinely equivalent.

Levers that DON'T work: shrinking the read() stub (removing the i>=RK_MAX guard / post-increment made the plain threshold go UP, not down — leaner source ≠ fewer symex steps; also adds array-bounds VCs). Global init is immovable (can't edit kilo.c globals; pipeline doesn't slice them). Can't raise --depth (rule: no hard-coding CLI args). So 1/21 is the canonical ceiling for this function; no spec edit improves it.
