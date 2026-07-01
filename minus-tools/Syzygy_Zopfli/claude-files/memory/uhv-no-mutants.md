---
name: uhv-no-mutants
description: "UpdateHashValue (zopfli.c) — avocado generates 0 mutants, kill score inherently 0; verifies with is_fresh + val-range invariant + exact closed-form ensures"
metadata: 
  node_type: memory
  type: project
  originSessionId: 41056ccd-581a-4f34-ba7c-c47b65e83732
---

`UpdateHashValue(ZopfliHash *h, unsigned char c)` does `h->val = (((h->val) << HASH_SHIFT) ^ c) & HASH_MASK;`. get-mutants reports "no mutable operators" → 0 mutants → kill score inherently 0, don't chase. Same regime as [[zgdseb-no-mutants]], [[getdynamiclengths-no-mutants]], [[zibs-precondition-kill]] neighbors.

**Spec that verifies** (enforce-contract, --depth 200, no harness needed — leaf, no loops):
- `requires(is_fresh(h, sizeof(*h)))`
- `requires(h->val >= 0 && h->val <= HASH_MASK)` — needed so the `<< HASH_SHIFT` (5) can't signed-overflow; this is the invariant the function itself maintains (ZopfliResetHash inits val=0).
- `assigns(h->val)`
- `ensures(h->val == ((((__CPROVER_old(h->val)) << HASH_SHIFT) ^ c) & HASH_MASK))` — exact closed form.
- `ensures(h->val >= 0 && h->val <= HASH_MASK)`

Verify script: /app/_verify_uhv.sh
