---
name: cbmc-printblocksplitpoints-vacuity
description: PrintBlockSplitPoints in zopfli.c verifies but 0/24; in-body ZOPFLI_APPEND_DATA in scan loop exhausts depth so body+assert unreachable
metadata: 
  node_type: memory
  type: project
  originSessionId: e3828765-8aa4-4b8d-b29a-a365129e250b
---

PrintBlockSplitPoints (zopfli.c ~line 4626) verifies but kills 0/24 — depth vacuity, same family as [[cbmc-tracebackwards-append-depth-vacuity]]. The scan loop contains an in-body `ZOPFLI_APPEND_DATA` (malloc/realloc/memset) whose allocation model exhausts --depth 200, so the loop body and the trailing `assert(npoints == nlz77points)` are never effectively reached. Proof: a mutant changing `nlz77points > 0` to `nlz77points < 0` (always-false for unsigned → skips body → leaves npoints==0 != nlz77points, which should fail the assert) still "Verifies" — the assert is unreachable. All 24 survivors lie on that unreached path; the only other effect is fprintf to stderr, which CBMC can't observe.

Unlike [[cbmc-addsorted-malloc-body-crash]], the append is inside a loop body so goto-instrument does NOT crash (cf. TraceBackwards). Strong sound spec left in place: is_fresh(lz77), lz77->size<8, is_fresh dists/litlens arrays, nlz77points<8, is_fresh lz77splitpoints array, empty __CPROVER_assigns() (writes only the local freed splitpoints buffer).
