---
name: zilz77s-no-mutants
description: "ZopfliInitLZ77Store — pure struct initializer, avocado generates 0 mutants; kill score inherently 0"
metadata: 
  node_type: memory
  type: project
  originSessionId: b8e1a5e3-93ac-421f-a921-7bf00671cbee
---

`ZopfliInitLZ77Store` in /app/Syzygy_Zopfli/c_code/zopfli.c is a straight-line struct initializer (9 field assignments, no operators). `get-mutants` reports "No mutant(s) generated (no mutable operators)", so kill score is inherently 0 — don't chase.

Verifies SUCCESSFUL @ depth 200 with the strongest possible spec: `is_fresh(store)` + `assigns(object_whole(store))` + an exact-value `ensures` per field (`size==0`, all pointers `==NULL`, `data==data`). Same shape as [[zibs-precondition-kill]] / [[zclz77s-no-mutants]] family of Init/Clean funcs.

**Why:** like other no-mutable-operator funcs, the 0 kill score is not a weak-spec signal. **How to apply:** verify with the exact-field contract and move on.
