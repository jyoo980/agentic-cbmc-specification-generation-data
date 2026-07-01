---
name: cbmc-warmuphash-replacement-vacuity
description: "ZopfliWarmupHash verifies but 0/8; exit unreachable in enforce harness when UpdateHashValue's contract is replaced twice"
metadata: 
  node_type: memory
  type: project
  originSessionId: 354c1897-242d-4b78-a454-b2a979122225
---

`ZopfliWarmupHash` in zopfli.c verifies (exit 0) but mutation testing kills 0/8.

**Why:** NOT depth/is_fresh-count vacuity (only 2 is_fresh objects: `h`, `array`;
bounding `end <= 8` did not help). A deliberately false postcondition
`h->val == 999999` still "Verifies", and `--cover location` shows the body blocks
(lines 2848-2849) are unreachable — the function exit where postconditions are
checked is never reached. The cause is CBMC's `--replace-call-with-contract
UpdateHashValue` applied to the *two* sequential `UpdateHashValue(h, ...)` calls
in the body (avocado always replaces callee contracts; inlining is impossible —
goto-instrument errors "Function does not have a contract"). I confirmed the
vacuity is intrinsic to the double replacement: it persisted after switching
UpdateHashValue's requires from `is_fresh(h)` to `__CPROVER_w_ok(h,...)`, and
after stripping the `__CPROVER_old` formula down to a range-only ensures. Same
family as [[cbmc-updatehash-vacuity]] and the other hash/cache vacuities
([[cbmc-depth200-isfresh-vacuity]]).

**How to apply:** Don't chase kills here — all 8 mutants (the `pos+1 < end`
branch relops and the `array[pos+0]`/`array[pos+1]` index arithmetic) are
unobservable because the exit is unreachable. A strong sound spec is in place:
requires pos < end, array fresh for `end` bytes, h fresh, h->val in [0,HASH_MASK];
assigns h->val; ensures the exact one-byte vs two-byte rolling-hash composition
of UpdateHashValue plus h->val in range. Leave it as-is.
