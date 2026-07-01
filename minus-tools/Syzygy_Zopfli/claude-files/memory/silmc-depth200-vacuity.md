---
name: silmc-depth200-vacuity
description: "StoreInLongestMatchCache 0/25 kills under hardcoded cbmc --depth 200; body unreachable, max 23/25 at depth 600+"
metadata: 
  node_type: memory
  type: project
  originSessionId: f12ee5b3-2c57-4d85-a286-489dc9acaf4f
---

StoreInLongestMatchCache (/app/Syzygy_Zopfli/c_code/zopfli.c, ~line 2739) already
verifies at the harness's `--depth 200` (_verify_silmc.sh: goto-cc -D__NO_CTYPE,
add-library, --partial-loops --unwind 5, replace-call-with-contract
ZopfliSublenToCache + enforce-contract). Kill score is **0/25 at depth 200** —
inherent [[avocado-depth200-vacuity]], do not chase.

**Why:** the prologue (6 is_fresh assumes on s, s->lmc, length/dist/sublen arrays +
sublen param, plus the assigns-clause entry snapshot of the large lmc->sublen
object_from region) costs ~450 depth before ANY body statement runs. Even mut0
(lmcpos = pos+blockstart OOB at the FIRST body line 2796) only kills at depth
~450-500; the body asserts (2801/2804) kill at ~550; the write mutants at ~600.

**How to apply:** measured ceiling is **23/25 at depth 600-800** (kill_silmc.sh,
run with --depth bumped). The 2 permanent survivors are equivalent mutants:
- mut11 `assert(length==1 || dist==0)` — on !cache_available path precondition
  forces length==1 && dist==0, so `||` ≡ `&&`.
- mut22 `assert(!(length==1 && dist!=0))` — after the write length[lmcpos] is
  either 0 or >=3, never 1, so the `==1` conjunct is dead; `!=0`/`==0` irrelevant.

Tried and FAILED to fit under 200: pinning pos==s->blockstart (constant sizes) —
no change, cost is the is_fresh/assigns machinery not the sizes. Removing the big
sublen is_fresh #5 drops frontier to ~350 but breaks the callee requires-check
(ZopfliSublenToCache needs is_fresh(lmc->sublen)) — unsound. r_ok/w_ok swap loses
the array separation the postcondition needs (same as [[zstc-depth200-vacuity]]).
Don't change the spec; it's already strong. Script: /app/kill_silmc.sh.
