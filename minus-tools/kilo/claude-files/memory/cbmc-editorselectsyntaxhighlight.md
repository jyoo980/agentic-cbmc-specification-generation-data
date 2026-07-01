---
name: cbmc-editorselectsyntaxhighlight
description: editorSelectSyntaxHighlight verifies but canonical depth-200 kill score is structurally 0 (global-init floor ~230 > 200; strstr unmodeled)
metadata: 
  node_type: memory
  type: project
  originSessionId: 04a70e61-91e4-482a-8a90-80133e9e969f
---

`editorSelectSyntaxHighlight` (kilo.c) VERIFIES canonically (depth-200 retry path: goto-cc -D__NO_CTYPE → --add-library → --partial-loops --unwind 5 → --enforce-contract → cbmc --depth 200 = SUCCESSFUL) but scores **0/10 canonically**, and that cap is **structural**, not a spec weakness.

**CORRECTED 2026-06-26** (prior note blamed a ~230 "global-init floor" — that was WRONG). Re-measured reach-depths with current spec (attempt-1, no --add-library, --partial-loops --unwind 5):
- filename-pinning-only contract → strstr (line 721) first reached at depth **~115**. So global init is NOT the bottleneck (~115, not 230).
- Adding `is_fresh(HLDB[0].filematch,16)`+`[1]==NULL` → reach ~145 (+30).
- Adding the nested pattern pin `is_fresh(filematch[0],2)`+strlen-on-valid-pattern → reach **~195** (+50). This pattern pinning + strlen execution is the real cost, and it's REQUIRED (gives strstr a valid 1-char needle + makes the postcond exact).
- The OBSERVABLE postcond violation (strstr stub runs → E.syntax assigned → return → ensures checked) isn't reachable until depth **~280–300** even with the cheapest correct strstr stub (single-loop, 1-char-needle model). A general nested-loop strstr pushes it to ~360.

So the real blocker is the **combined contract-setup(~195) + strstr-stub-execution + postcond cost (~280–300) ≫ 200**, not a global-init floor. There is no way to fit setup+strlen+strstr+assign+return+postcond in 200 steps at unwind 5. Confirmed canonically via `run_mutants_editorSelectSyntaxHighlight.sh 200`: BASELINE SUCCESS, **0/10 killed**.

**strstr stub does NOT rescue it.** strstr IS unmodeled canonically (`--add-library` provides strlen but not strstr; attempt-2 path = vacuous SUCCESS at 200 because strlen's library model pushes strstr-reach to ~240). A `/app/stubs/*.c` with `/* FUNCTION: strstr */` (picked up by get_stub_paths_for since strstr is an external callee) makes attempt-1 authoritative and baseline still verifies at 200 — but still VACUOUS (postcond unreached) → still 0/10. And a 1-char-only strstr model is unsound for editorFind (the only other strstr caller, itself canonically unverifiable). Net: no score gain, added risk → do NOT install. Spec left unchanged.

Spec design that DOES verify (kept in kilo.c): havoc-proof the globals by pinning `HLDB[0].filematch` to a controlled 2-slot list with ONE single-char pattern via nested is_fresh (`is_fresh(HLDB[0].filematch, 2*sizeof(char*))`, `filematch[1]==NULL`, `is_fresh(filematch[0],2)`, `filematch[0][1]=='\0'`, `filematch[0][0]!='\0'`); filename = is_fresh 3 bytes, `[2]=='\0'`, `[0]!='\0'`; `E.syntax==NULL`; `assigns(E.syntax)`. A 1-char pattern + 2-char filename makes strstr's result a finite function of 2 byte-compares, giving an EXACT ensures: `E.syntax==HLDB` iff `(f0==P && (P!='.' || f1=='\0')) || (f0!=P && f1==P)` where P=`filematch[0][0]`. This exactly characterizes select-vs-not and would kill 8/10 (the 2 survivors `j!=` and `HLDB-j` are equivalent at HLDB_ENTRIES==1) IF the body were reachable. Scorer: `kilo/run_mutants_editorSelectSyntaxHighlight.sh` (mirrors run_cbmc two-attempt retry).

Note string.c stub (strlen/memset/realloc) has NO `/* FUNCTION: */` markers, so build_stub_index never picks it up — it's NOT in the canonical pipeline.
