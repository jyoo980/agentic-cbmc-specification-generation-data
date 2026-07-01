---
name: zclz77s-no-mutants
description: ZopfliCleanLZ77Store is a 7-free func; avocado generates 0 mutants (kill score inherently 0); verifies with full is_fresh+frees+assigns+was_freed contract
metadata: 
  node_type: memory
  type: project
  originSessionId: d69b73e9-c493-48cf-9e10-0e864f1d1d64
---

ZopfliCleanLZ77Store (Syzygy_Zopfli/c_code/zopfli.c) frees 7 pointers:
litlens, dists, pos, ll_symbol, d_symbol, ll_counts, d_counts.

get-mutants reports "No mutant(s) generated (no mutable operators)" —
kill score inherently 0, nothing to chase. Same family as [[zcc-no-mutants]]
and [[zcbs-no-mutants]].

**How to apply:** verify with the standard free-func contract pattern — for the
struct ptr and all 7 array ptrs: __CPROVER_requires(is_fresh(...)), one
__CPROVER_assigns(object_whole(...)) listing all 7, one __CPROVER_frees(...)
listing all 7, and __CPROVER_ensures(was_freed(__CPROVER_old(...))) per ptr.
Verifies SUCCESSFUL at --depth 200 via _verify_zclz.sh (goto-cc + enforce-contract).
