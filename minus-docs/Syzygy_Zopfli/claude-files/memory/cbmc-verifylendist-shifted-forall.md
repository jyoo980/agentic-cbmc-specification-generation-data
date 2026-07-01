---
name: cbmc-verifylendist-shifted-forall
description: "ZopfliVerifyLenDist in zopfli.c cannot verify; CBMC won't instantiate a shifted-index __CPROVER_forall to discharge the body's equality assert"
metadata: 
  node_type: memory
  type: project
  originSessionId: 91bcee01-c7a7-4249-be78-5cce61d44af6
---

`ZopfliVerifyLenDist(data, datasize, pos, dist, length)` in
/app/Syzygy_Zopfli/c_code/zopfli.c does NOT verify under CBMC, and this appears
fundamental, not a spec-tuning issue.

The body asserts `data[pos-dist+i] == data[pos+i]` inside `if (data[pos-dist+i] != data[pos+i])`
for `i` in `[0,length)`. That assert is reachable iff some byte pair differs, so the
ONLY way to discharge it is a validity precondition forcing equality for all `i<length`,
and CBMC must *use* that forall to prove the `if` guard is always false.

Two things I confirmed:
- **Overflow was the original blocker for memory-safety.** `pos` is an unbounded
  `size_t`; `pos + length <= datasize` is satisfiable by wraparound, letting
  `data[pos+i]` read far outside the object. Fix: express bounds overflow-safe —
  `__CPROVER_requires(pos <= datasize)` + `__CPROVER_requires(length <= datasize - pos)`
  + `__CPROVER_requires(dist <= pos)`. After this, ALL pointer-OOB failures vanished.
- **CBMC will not instantiate the shifted-index forall.** With
  `__CPROVER_forall { size_t k; (k<length) ==> data[pos-dist+k]==data[pos+k] }` as a
  requires, both the body asserts AND an identical ensures still FAIL. The working
  idiom elsewhere in this file (CalculateBlockSymbolSizeSmall, ~line 253) discharges
  body asserts from a forall only because it uses a DIRECT index `litlens[qi]` that
  matches the loop var `i` exactly. Shifted indices (`pos-dist+k`, `pos+k`) don't get
  E-matched/instantiated. Tried: body-matching `k`-form, whole-index `j`-form
  (`data[j]==data[j+dist]`), unique quantifier names (ka/kb), single-line formatting —
  none discharge the asserts.

Conclusion: left the strongest SOUND spec in place (overflow-safe bounds + validity
forall in both requires and ensures). It fails to verify purely due to this CBMC
quantifier-instantiation limitation (CLAUDE.md: "CBMC cannot verify all correct C code").
Related: [[cbmc-partialloops-unwind5-truncation]], [[cbmc-findlongestmatch-vacuity]].
