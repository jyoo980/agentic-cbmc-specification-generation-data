---
name: zdef-depth200-vacuity
description: "ZopfliDeflate verifies @200 but 0/18 kills (vacuity); maximally strong (7/18 @1000), rest equivalent, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: f984d9ad-0219-4461-9f0f-90f08895f085
---

ZopfliDeflate (top-level master-block driver, chops input into ZOPFLI_MASTER_BLOCK_SIZE chunks → ZopfliDeflatePart). Sibling of [[zdp-depth200-vacuity]] and the whole driver family ([[alzbat-depth200-vacuity]], [[zcbsat-depth200-vacuity]], [[zbs-depth200-vacuity]], [[zlz77opt-depth200-vacuity]]).

**Spec (sound, verifies @depth200):** pin btype==0 single-master-block regime (`insize <= ZOPFLI_MASTER_BLOCK_SIZE` so the do/while makes exactly ONE ZopfliDeflatePart call — a 2nd call is unprovable anyway: ZDP ensures *bp==0 but requires *bp in 1..7). Forward ZDP's own precondition (is_fresh options/in[insize]/bp(1..7)/outsize(*outsize==1)/out/*out), assigns(*bp,*outsize,*out,(*out)[*outsize-1]), ensures(*bp==0). ZDP is the only replaced callee; fprintf is nondet external.

**0/18 kills @depth200 = vacuity**, NOT weak spec. Proven maximally strong: **7/18 @depth1000** (`kill_zdef_depth.sh 1000`, official-flow mirror: goto-cc + `--partial-loops --unwind 5` + `--replace-call-with-contract ZopfliDeflatePart --enforce-contract ZopfliDeflate` + `cbmc --depth N`). The 7 killable: 3 masterfinal cmp mutants (`<`,`<=`,`==` → push forwarded inend past insize so is_fresh(in,inend) precond-assert fails) + 1 index mutant (`i+size`→`i-size`, inend underflows) — these die @depth600; plus 3 while-guard mutants (`<=`,`>=`,`==` → force a 2nd ZDP call whose *bp/​*outsize precond fails) — these need depth>600 (die @1000).

**11 survivors are genuinely equivalent/unobservable** (don't chase): while `>`/`!=` (single-iter, i==insize, both end loop = original); masterfinal `>`/`!=` (differ only at insize==MASTER on final2, unobservable on *bp); `i-MASTER` (i=0, underflow still >=insize); `size=insize+i` (i=0); `final||masterfinal` (masterfinal=1, unobservable); 4 verbose fprintf-arg mutants (no postcond can observe printed output).

Depth-200 frontier cause: harness is_fresh prologue (8 requires) + first ZDP precondition assertions land past depth 200, exactly like ZDP. r_ok lever can't apply — ZDP's requires use is_fresh(in), so my `in` must stay is_fresh (r_ok-sourced pointer fails ZDP's is_fresh-as-assertion). Scripts: `/app/kill_zdef.py` (official @200), `/app/kill_zdef_depth.sh` (depth-parametric).

**Levers re-confirmed dead (2026-06-28 re-attempt):** (1) Pinning `insize == 1` (tiny is_fresh(in,1) object instead of <=1M) still verifies but still 0/18 @200 — frontier is the prologue INSTRUCTION COUNT (8 is_fresh requires + ZDP contract-replacement precond asserts), NOT object size, so shrinking insize doesn't pull the iter-1 ZDP precond under depth 200; reverted (also weakens generality). (2) Loop contracts (the [[zic-dfcc-loopcontracts-12of16]] vacuity-beater) won't help either: they don't shrink the prologue, and the killable mutants all need an iter-1 (masterfinal/index) or iter-2 (while-guard) body assertion that stays past the prologue-dominated frontier. Confirmed: this is genuinely don't-chase.
