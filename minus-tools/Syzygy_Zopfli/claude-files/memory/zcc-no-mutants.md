---
name: zcc-no-mutants
description: "ZopfliCleanCache verifies w/ frees contract; avocado generates 0 mutants, kill score inherently 0"
metadata: 
  node_type: memory
  type: project
  originSessionId: 025ce0e4-53bf-4aba-9641-811589b9c5b5
---

`ZopfliCleanCache` in /app/Syzygy_Zopfli/c_code/zopfli.c just `free()`s lmc->length, lmc->dist, lmc->sublen. get-mutants reports "No mutant(s) generated (no mutable operators)" — kill score is inherently 0, nothing to chase.

Verifies with a frees contract: is_fresh on lmc and each of the 3 member pointers, then `__CPROVER_assigns(__CPROVER_object_whole(lmc->length), ...)` for the 3 freed objects, `__CPROVER_frees(lmc->length, lmc->dist, lmc->sublen)`, and `was_freed(__CPROVER_old(...))` ensures.

**Why:** an empty `__CPROVER_assigns()` + frees FAILS with "Check that __CPROVER_POINTER_OBJECT(lmc->length) is assignable: FAILURE" — free needs the freed object in the assigns set (via object_whole), not just the frees set.

**How to apply:** free-only cleanup functions need BOTH frees() AND assigns(object_whole(...)) of the same pointers. Verify script: /app/_verify_zcc.sh. Same no-mutant pattern as [[zgdseb-no-mutants]] and [[getdynamiclengths-no-mutants]].
