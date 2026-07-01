---
name: cbmc-editorrowappendstring
description: editorRowAppendString verifies canonically but scores 0/3 — body unreachable at --depth 200; one mutant is equivalent
metadata: 
  node_type: memory
  type: project
  originSessionId: 4886c61d-1850-444a-ac41-cd2367d4b612
---

`editorRowAppendString` (kilo.c) VERIFIES under canonical run_cbmc (PASS) but its
canonical kill score is **0/3**, and that is the ceiling — same depth limitation as
[[cbmc-editoropen-canonical-zero-kills]] and [[cbmc-depth-vacuous-and-recursive-callees]].

Why 0/3 is unavoidable canonically:
- All 3 avocado mutants are on/after the `realloc` line (realloc size+len±1, memcpy +size->-size).
- The contract-enforcement PREFIX — is_fresh setup for the struct + 3 buffers (row, chars, s, hl)
  plus the `editorUpdateRow` `--replace-call-with-contract` machinery — exhausts the `--depth 200`
  budget BEFORE the body's first statement. Body-start is reachable only at `--depth >~260`
  (probe: insert `__CPROVER_assert(0,...)` as first body stmt; SUCCESS=vacuous at 200, FAILURE at 300).
  So the whole body is vacuous; every mutant is reported PASS (survives).
- Cheap malloc/realloc stubs (`__CPROVER_allocate`) do NOT help: the hog is the contract harness,
  not allocation cost. Don't add a marked realloc/memcpy stub to /app/stubs — build_stub_index would
  link it into editorUpdateSyntax (realloc) and editorOpen (memcpy), which are already verified, risking them.

The memcpy `+size->-size` mutant is additionally an EQUIVALENT mutant: editorUpdateRow's contract
requires post-`row->size == 1`, forcing `row->size == 0` at the memcpy, where `chars-0 == chars+0`.

The spec written is still strong (would kill the 2 realloc mutants at non-truncating depth): it pins
`__CPROVER_OBJECT_SIZE(row->chars) == old(size)+len+1` (the only signal that survives no-bounds-check
cbmc — under-allocation is otherwise invisible), the copied content, size, and dirty-flag deltas.
Preconditions must satisfy editorUpdateRow's replaced contract: row->size==0 & len==1 (post-size 1),
render==NULL, is_fresh(hl,8), E.syntax==NULL.

RE-CONFIRMED 2026-06-26 via probe matrix (insert `__CPROVER_assert(0)` as first body stmt, vary contract):
the >200-depth cost is the ENTRY harness (requires + assigns write-set setup), NOT the editorUpdateRow
replacement (which sits at the body's tail). The three `is_fresh` on row / row->chars / row->hl are JOINTLY
load-bearing: dropping any ONE of them makes goto-instrument CRASH (GOTO-INSTRUMENT_FAILED), because the
assigns clause references row->chars/object_whole and row->hl/object_whole and needs those pointers resolvable.
Dropping `is_fresh(s,len)` alone is the only safe removal and it does NOT make the body reachable. Removing
`object_whole(...)` from assigns is safe (no crash) but also doesn't free enough depth. Net: no valid,
verifying contract reachable at --depth 200 -> 0/3 confirmed as the hard canonical ceiling; nothing to add.

Helper scripts: /app/kilo/_run_canon_eraps.py (canonical verdict), /app/kilo/_mut_eraps.py (3-mutant harness),
/app/kilo/_probe_eraps.py + /app/kilo/_probe2.py + /app/kilo/_probe3.py (reachability/depth-hog probes).
Key mechanism: run_cbmc's first attempt (no --add-library) uses cbmc's builtin-library realloc/memcpy and
PASSES; only a FAILED first attempt triggers the --add-library retry that crashes goto-instrument
(the malloc should_malloc_fail invariant) — so a verifying spec must pass on attempt 1.
