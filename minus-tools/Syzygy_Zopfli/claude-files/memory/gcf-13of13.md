---
name: gcf-13of13
description: GetCostFixed 13/13 kills via exact closed-form cost spec; needed clz-form rewrite of ZopfliGetDistExtraBits ensures (inequality form overflows under replace-call)
metadata: 
  node_type: memory
  type: project
  originSessionId: 13e1f52d-1a57-45e1-9fd1-10ce734365c3
---

GetCostFixed (fixed-tree cost model) verifies @depth200 with a PERFECT 13/13 kill score — no equivalent mutants, no vacuity. Leaf-ish, no loops.

Contract: requires(litlen<=258 && dist<=32768); assigns(); two regime ensures:
- dist==0 ==> return == (litlen<=143 ? 8 : 9)  — kills all 5 `litlen<=143` cmp mutants.
- dist!=0 ==> return == 5 + (litlen<=114 ? 7 : 8) + ((litlen<=10||litlen==258)?0:((31^__builtin_clz(litlen-3))-2)) + (dist<5?0:((31^__builtin_clz(dist-1))-1))
  - `litlen<=114` is the exact image of `lsym<=279` (ZopfliGetLengthSymbol: lsym in 280..285 ⟺ litlen in [115,258]) — kills all 5 `lsym<=279` mutants.
  - the exact +dbits/+lbits terms kill the two sign mutants.
  - the dist==0->dist!=0 mutant routes the literal case into ZopfliGetDistExtraBits(0), violating its requires — killed at the replace-call assertion.

KEY GOTCHA: had to change a callee. ZopfliGetDistExtraBits's ensures used the INEQUALITY form `(1<<(R+1))<=dist-1 && dist-1<(1<<(R+2))`. That is NOT replacement-safe: under --replace-call-with-contract the return value R is havoc'd unconstrained, so the shift `1<<(R+2)` triggers overflow/undefined-shift FAILUREs in the CALLER (GetCostFixed). Adding a separate bound ensures does NOT fix it (the whole ensures conjunction is evaluated for well-definedness before being assumed). Fix: rewrite to the closed clz form `R == (31^__builtin_clz(dist-1))-1` (same form ZopfliGetLengthExtraBits already uses — no shift on R) + a global `0<=R<=13` bound first. ZopfliGetDistExtraBits still verifies and still 7/7 kills; the clz form also matches GetCostFixed's postcond syntactically so the proof is trivial. Lesson: ensures with `1<<(returnvalue+k)` are replacement-poison; prefer clz/closed forms that put no arithmetic on the havoc'd return value.

Kill scripts: /app/_kill_gcf.py, /app/_kill_zgdeb.py. Related: [[zgdebv-equivalent-mutants]], [[gls-6of6]].
