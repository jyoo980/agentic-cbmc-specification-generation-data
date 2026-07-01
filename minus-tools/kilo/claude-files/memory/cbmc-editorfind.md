---
name: cbmc-editorfind
description: "editorFind can't verify canonically — goto-instrument 6.9.0 crashes replacing the variadic editorSetStatusMessage contract; spec verifies when that one replace is skipped"
metadata: 
  node_type: memory
  type: project
  originSessionId: 6187180f-f8fc-4b6d-88ae-ebd8a3638dbb
---

editorFind (kilo.c) is **structurally unverifiable under the canonical run_cbmc pipeline**, for two reasons (the first is the hard blocker):

1. **Variadic-contract replace crashes goto-instrument 6.9.0.** editorFind's in-file callees are editorReadKey, editorRefreshScreen, editorSetStatusMessage. The pipeline does `--replace-call-with-contract` on all three. `editorSetStatusMessage(const char *fmt, ...)` is **variadic**, and replacing *any* contract on it aborts goto-instrument (`Invariant check failed ... variables.size() == values.size()` in std_expr.cpp instantiate, preceded by a red-herring `__CPROVER_replace_ensures_is_fresh: function 'malloc' is not declared`). Proven minimal: even a trivial `requires(1)/assigns(statusmsg_time)/ensures(1)` on it crashes; removing the contract instead gives a clean "does not have a contract" error. `--add-library` does NOT help. The crash output contains neither "no body for callee/function" nor "Recursive call", so run_cbmc's retries don't fire → GOTO_INSTRUMENT_FAILED → UNVERIFIED. No editorFind contract can avoid it (crash is at the replace step, independent of editorFind's own spec). editorFind is the first verified function to *call* editorSetStatusMessage, so no earlier function hit this.

2. **editorReadKey's read model is single-shot.** Its contract `requires(rk_idx==0)` and `assigns(rk_idx)` (havoc). editorFind calls it once per `while(1)` iteration; after the first call rk_idx is nondet, so a 2nd iteration's `rk_idx==0` assertion fails. So only ONE loop iteration is sound — the function must exit on iteration 1 (ESC/ENTER). That makes the whole search block (lines ~1659-1693, the bulk of the 34 mutants) unreachable in any verifying config.

**The spec I wrote IS correct and strong for the reachable slice.** It pins the single-row editorRefreshScreen config (same as that callee's precondition), forces the first scripted key to ESC (`rk_idx==0, rk_ret[0]==1, rk_byte[0]==ESC, rk_ret[1]==0, rk_ret[2]==rk_ret[3]==0`) so editorFind takes its restore-cursor/clear-status/return exit on iteration 1, assigns {E.cx,cy,coloff,rowoff, object_whole(statusmsg), statusmsg_time, rk_idx}, and ensures cursor/viewport/numrows == old (ESC restores them). **Verified VERIFICATION SUCCESSFUL** via the pipeline that replaces only editorReadKey+editorRefreshScreen and lets editorSetStatusMessage run with its real body — i.e. it would verify if not for blocker #1. Canonical kill score stays 0 because goto-instrument never gets past the variadic replace.

Repro scripts kept: `/app/kilo/_canon_find.sh` (full canonical, crashes) and `/app/kilo/_cg_find.py` (callee list). Related: [[cbmc-editorsetstatusmessage]] (the variadic callee, 0 mutants), [[cbmc-editorreadkey]] (single-shot rk_idx model), [[cbmc-editorrefreshscreen]] (strict single-row precondition).
