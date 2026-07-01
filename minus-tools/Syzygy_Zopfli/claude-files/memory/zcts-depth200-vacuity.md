---
name: zcts-depth200-vacuity
description: "ZopfliCacheToSublen verifies but scores 0/21 at depth 200 (hard vacuity); spec is strong (16/21 @depth600), don't chase"
metadata:
  node_type: memory
  type: project
  originSessionId: minus-tools
---

ZopfliCacheToSublen (zopfli.c ~line 2809) — nested loop (outer j<ZOPFLI_CACHE_LENGTH,
inner i<=length writing sublen[i]=dist), calls ZopfliMaxCachedSublen at line 2833 BEFORE
the `if(length<3)` guard. Verifies under the official pipeline (run_cbmc) with the existing
spec: is_fresh(lmc)+pos<=1024+is_fresh(lmc->sublen, CACHE_LEN*pos*3+CACHE_LEN*3 window)
+is_fresh(sublen,259)+assigns(object_whole(sublen)) + exact-value ensures pinning sublen[0]
(`length<3 || sublen[0]==lmc->sublen[..*3+1]+256*lmc->sublen[..*3+2]`). Callee
ZopfliMaxCachedSublen is replace-call-with-contract (scorer auto-detects in-file callees).

**Scores 0/21 at the hardcoded depth 200 — pure depth-vacuity, NOT a weak spec.**
Proof the spec is strong: same spec/pipeline at `cbmc --depth 600` kills **16/21**
(dist mutants, length<3 guard ×5, length=cache-3, most loop-guard mutants). 5 survivors
even @600 are equivalent/unwind-bound (j<=CL & j!=CL equiv under break; i<length edge;
prevlength=length-1; length!=maxlength — would need a postcondition pinning more of sublen[]).

**Why 0 at depth 200 and why no lever recovers it:**
- The exit postcondition is the ONLY observable spec point (no in-body asserts, unlike its
  sibling [[zstc-depth200-vacuity]] which has source assert()s and thus gets 10/38). Every
  mutant's effect is visible only at that exit postcondition.
- Depth-to-reach the postcondition: ~300 (guard, early-return path) / ~350 (dist, needs a
  loop iteration). Confirmed: dist mutant SUCCESSFUL@200/250/300, FAILED@350.
- The hog is the mandatory **ZopfliMaxCachedSublen contract-call at line 2833** (before the
  length<3 guard), whose is_fresh-heavy precondition is asserted at the call site. Even the
  early-return path (no loop) needs depth 300. Prologue trimming does NOT help: tested
  pos==0 (kills symbolic offset, shrinks lmc->sublen window to 24 bytes) and smaller
  windows — dist mutant still SURVIVES@200. So allocation cost isn't the bottleneck; the
  callee contract assertion is.
- **r_ok trick can't apply** (unlike [[zlz77gbr-rok-beats-vacuity]]): the replaced callee
  ZopfliMaxCachedSublen requires is_fresh(lmc) AND is_fresh(lmc->sublen,window), asserted at
  the call site, so the caller MUST provide is_fresh for both — can't downgrade to r_ok.
  Downgrading zmcs's own contract to r_ok would break its 5/5 ([[zmcs-5of5]]).
- Loop contracts are inert under the avocado scorer (no --apply-loop-contracts), per
  [[zstc-depth200-vacuity]].

Conclusion: 0/21 is the inherent ceiling under the fixed depth-200 scorer. Spec is already
maximal/strong; left unchanged. Don't chase. Scripts: /app/_verify_zcts.sh, /app/kill_zcts.py
(default depth 200); /app/_diag_zcts/kill600.py demonstrates 16/21 at depth 600.
