---
name: cbmc-abappend
description: abAppend verifies but canonical depth-200 scores 0/3; builtin realloc (~350 steps) truncates the body; spec is 3/3-strong at depth 500
metadata: 
  node_type: memory
  type: project
  originSessionId: 5dd2eda4-68e4-4f85-a76e-cf5fe9ba9f21
---

`abAppend` (kilo.c) — 3 mutants: inverted NULL check, `memcpy(new-ab->len,...)`, `realloc(ab->len-len)`. All 3 are body-internal memory bugs.

Spec written: is_fresh(ab), is_fresh(ab->b, ab->len), is_fresh(s, len), `ab->len>0 && <=8`, `len>0 && <=8`, `assigns(ab->b, ab->len)`, `frees(ab->b)` (realloc frees ab->b), ensures len unchanged OR +len. Verifies at canonical `--depth 200` (vacuously).

**Root cause of 0 kills (same family as [[cbmc-editorrowappendstring]], [[cbmc-editoropen-canonical-zero-kills]]):** CBMC's *builtin* realloc alone costs ~350 depth steps — SIZE-INDEPENDENT (tested ab->len==1 and <=8, both unreachable at depth 200/300; body first reachable ~400). So the body past `realloc` is depth-truncated → every memcpy/null-check property is vacuous → all 3 mutants survive AND original passes vacuously.

**Proof the spec is strong, not weak:** the body becomes reachable at a precise threshold — depth 270 → 0/3, depth 280 → 3/3 (re-measured 2026-06-26; earlier "~400/500" estimate was too high). At depth >=280 the committed spec verifies the original and KILLS all 3 mutants (3/3). Small bounds (<=8) chosen to reach the body at the lowest depth; `len>=1 && ab->len>=1` needed so mutants 2/3 are non-equivalent.

**Confirmed (2026-06-26) no spec change reaches the body within depth 200:** the builtin-realloc cost is the floor. Tried `ab->len==1 && len==1` (exact, smallest) → still 0/3 at 200 (size-INDEPENDENT, as noted). Tried `ab->b==NULL` (realloc-as-malloc, drops is_fresh(ab->b) so no copy/free) → cheaper but threshold only falls to ~250 (depth 240 → 0/3, 260 → 3/3), still above 200. Everything observable (memcpy offset, null-check, ab->len update, postcondition at return) sits *past* the realloc builtin, so no assigns/frees/ensures trick kills a mutant before the depth-200 cutoff. Spec is already maximal; left kilo.c unchanged.

**Why no stub:** the only way to reach the body at depth 200 is a lean realloc/memcpy model, but the stub index ([[stubs.py]]) keys off `/* FUNCTION: name */` globally — a realloc/memcpy stub would be linked into EVERY realloc/memcpy caller. Confirmed warning in [[cbmc-editorrowappendstring]]: don't add global realloc/memcpy stubs. stubs/string.c has a realloc model but NO marker, so it is never linked (dead).
