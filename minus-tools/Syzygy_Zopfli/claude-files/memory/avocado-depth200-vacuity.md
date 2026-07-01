---
name: avocado-depth200-vacuity
description: "avocado CBMC pipeline hardcodes --depth 200, making loop-heavy functions' checks vacuous so kill score is 0 regardless of contract"
metadata: 
  node_type: memory
  type: project
  originSessionId: 453b59e7-0a99-463c-afbb-c75812e354b3
---

The avocado verification pipeline (`tools/run_cbmc.py`) hardcodes `cbmc --depth 200`
(and `goto-instrument --partial-loops --unwind 5`, `--enforce-contract`). For
loop-heavy functions whose fixed scaffolding already consumes ~200 symbolic-execution
steps, the `--depth 200` cutoff is reached BEFORE the loop body and BEFORE the
postcondition, so all in-loop memory-safety checks and the `__CPROVER_ensures` clause
become vacuous (a deliberately-wrong `ensures(x==99)` still "verifies").

**Consequence:** kill score is 0/N for ANY contract on such a function — a depth-bound
artifact, not a weak spec.

**Concrete case — `ExtractBitLengths` in `Syzygy_Zopfli/c_code/zopfli.c`:** the
`int counts[16]={0}` init + unwound chain sweep + three required `is_fresh` objects
cost ~200 steps; the first in-loop check lands at depth ~205, the postcondition at
~270. Verified the chosen contract kills 12/12 mutants at `--depth 600` but 0/12 at
`--depth 200`. The strongest contract was kept anyway (it fully captures behavior).

**Second case — `InitLists` in the same file:** all 5 mutants sit on the loop guard
`i < maxbits`. Contract (requires maxbits 1..3, is_fresh pool/leaves/lists sized to
maxbits, assigns + forall postcondition over rows) kills 4/5 unbounded — `i != maxbits`
is an equivalent mutant (i steps 0,1,2.. hits maxbits exactly). At `--depth 200`: 0/5,
verified the `i<=maxbits` OOB mutant survives at 200 but dies at 600. Same artifact.

**Reconfirmed 2026-06-26:** re-measured all 5 InitLists mutants under the real pipeline
(`--replace-call-with-contract InitNode`, unwind 5, `--depth 200`): all survive (0/5). Full
contract: `i<=` kill needs depth 500, `i>` (postcondition kill) needs >400. Tried a *stripped*
contract (dropped `forall` + `old()` pool-advance ensures + weight/tail ensures; kept only
is_fresh ×4, assigns, and `lists[0][0]->count==1 && [0][1]->count==2`): `i<=` dropped to ~250-400,
`i>` to ~300, but NEITHER reaches ≤200. The 4 distinct is_fresh objects + assigns frame checks alone
exceed 200 depth before any check is reachable, so no contract rewrite raises the score. Kept the
original strongest contract (restored byte-identical).

**Third case — `ZopfliLengthsToSymbols` in the same file (2026-06-26):** 4 sequential
loops + 2 `malloc`s. At `--depth 200` only loop-1 iteration-1 is reached (probed: end at
~depth 400, loop2 at ~220, loop4 at ~280); 0/35 mutants killed with the strongest spec.
BUT this one also needed a stub to verify AT ALL: malloc has "no body" → returns a nondet
pointer → every `bl_count[...]`/`next_code[...]` deref fails, AND the harness's missing-body
retry (`goto-instrument --add-library`) CRASHES the enforce-contract pass with an invariant
violation on the bundled malloc model's `should_malloc_fail` symbol (`no definite size for
lvalue target`). Fix: created `/app/stubs/cprover_alloc.c` modeling malloc as
`__CPROVER_allocate(size,0)` (fresh, non-NULL) + free as no-op. `tools/run_cbmc.py`
auto-discovers `*.c` in `/app/stubs` via `/* FUNCTION: <name> */` markers and compiles them
in, so the initial run succeeds and the crashing retry is never taken. The function went
UNVERIFIED→VERIFIED purely from the contract+stub; kill score stays 0 (depth ceiling).
The `read_stdin_to_bytes`-style functions avoid the crash only because they guard every malloc
deref with a NULL check, so their initial run succeeds before the retry triggers.

**Reconfirmed 2026-06-26 (ZLtS, second pass):** re-ran `killscore_zlts.py` → 0/35. Bisected the
loop-1 `i<=n` OOB mutant: full spec survives ≤207, dies at 210. Then stripped the requires to the
leanest that still type-checks (`n==1 && maxbits==1`, dropped `forall` and the `n<=3` range) — floor
only dropped to ~201-204; STILL survives at exactly depth 200, dies at 205. The ~205 prologue floor is
dominated by fixed costs the spec cannot touch: whole-file global-var nondet init (`goto-instrument`
prints "Adding nondeterministic initialization of static/global variables"), the enforce-contract
harness, and the two malloc-model calls. No requires rewrite gets the earliest divergence under 200, so
0/35 is firm. Kept original strongest contract (byte-identical, re-verified SUCCESSFUL at depth 200).

**Fourth case — `EncodeTree` in the same file (2026-06-26):** the largest loop-heavy
function here — init loop (19), two trim while-loops, a big main RLE loop with nested
whiles, then `ZopfliCalculateBitLengths`/`ZopfliLengthsToSymbols` calls + an output
section. Contract is SIZE-ONLY (`requires(out == 0)`): is_fresh ll_lengths[288]/d_lengths[32],
`forall` lengths <= 15 (so `unsigned char symbol` indexes clcounts[19] in bounds), use_* in
{0,1}, `assigns()` empty, `ensures(return >= 26)`. Verifies at depth 200. Kill score **0/136**.
Confirmed it's the depth ceiling three ways: (1) false `ensures(==123456789)` passes at 200,
fails at 1000 (threshold between 400 and 500); (2) an `assert(0)` probe at the top of the main
loop body is NOT reached at depth 200 (reached only by depth ~400) — at 200 execution is still
in the init/trim loops, and `--unwind 5 --partial-loops` stops the trim loops 5 iters before
their boundary OOB, so even the trim-guard mutants can't diverge. NOTE: `EncodeTree` can ONLY
verify vacuously regardless of depth — it calls `ZopfliCalculateBitLengths(.,19,7,.)` and
`ZopfliLengthsToSymbols(.,19,7,.)` but those callee contracts `require(n==1 && maxbits==1)`, so
under `--replace-call-with-contract` the n==1 precondition assertion at line ~1170 is
unsatisfiable; any non-vacuous proof reaching it fails. `out==0` is also load-bearing for
verification: with `out!=0` the main loop's `ZOPFLI_APPEND_DATA` (realloc, no stub) becomes
reachable → missing-body retry. Harness: `Syzygy_Zopfli/c_code/killscore_encodetree.py`
(parallel, per-mutant temp dirs) + `runcbmc.py` (single-function run_cbmc mirror).

**Fifth case — `AddLZ77Data` in the same file (2026-06-27):** verifies SUCCESSFUL at
`--depth 200`. 40 mutants, ALL on in-loop body asserts (litlen<256, ll_lengths[..]>0,
d_lengths[..]>0, litlen>=3&&<=288), the `if(dist==0)` branch, the loop guard `i<lend`,
and the final `testlength==expected_data_size` assert. Kill score **0/40** under the real
harness (`--partial-loops --unwind 5`, replace-call-with-contract on all 8 callees:
AddHuffmanBits, AddBits, ZopfliGet{Length,Dist}Symbol, ZopfliGet{Length,Dist}ExtraBits[Value],
`--depth 200`). Confirmed depth ceiling via `/app/kill_addlz77.py` (parses get-mutants
diffs, applies each, re-runs pipeline): the `assert(litlen==256)` mutant (clearly false in the
literal branch, litlen∈[11,34]) reports SUCCESS at depth 200/400 but FAILURE at depth 800. The
prologue floor (~600-800 steps) is FIXED — stays at 800 even after removing both `forall`
requires AND collapsing to the literal-only regime (dists[0]==0, single callee call). The cost
is the contract-enforcement preamble: 6 `is_fresh` objects (4 are 288/32-elem arrays) + the
`assigns(*bp,(*out)[*outsize-1])` frame instrumentation. No spec rewrite gets the earliest body
assert under depth 200. Spec is genuinely strong (kills these at depth≥800); left byte-identical.

**Sixth case — `AddNonCompressedBlock` in the same file (2026-06-27):** verifies
SUCCESSFUL at `--depth 200`. 26 mutants, ALL on the in-loop chunk emitter (lines
1765-1786: `if (pos+blocksize>inend)`, `blocksize=inend-pos`, `currentfinal=pos+blocksize>=inend`,
the `AddBit(final && currentfinal,...)` arg, the four length-byte `ZOPFLI_APPEND_DATA`
`%256`/`/256` expressions, the copy-loop guard `i<blocksize`, and `in[pos+i]`). Kill
score **0/26** under the real harness (`--replace-call-with-contract AddBit`,
`--partial-loops --unwind 5`, `--depth 200`). Confirmed depth ceiling: a false
`ensures(*bp==0 && *outsize==123456789)` PASSES at depth 200 but FAILS at depth 1000
(body `in[pos+i]` deref + the postcondition become reachable). Harness:
`Syzygy_Zopfli/c_code/killscore_anc.py`. NOTE: needed a **realloc stub** to verify at
all — `ZOPFLI_APPEND_DATA` calls `realloc` (not just malloc) once the 1-byte buffer
grows; realloc had no body in `/app/stubs/cprover_alloc.c`, so the missing-body
`--add-library` retry crashed goto-instrument on the bundled malloc model's
`should_malloc_fail` (same crash as ZLtS). Added a `realloc` success model
(`__CPROVER_allocate(size,0)`, no content copy) to `cprover_alloc.c`; first run then
succeeds and the crashing retry is never taken. Contract: AddBit-ready entry shape
(`*bp` in 1..7, `*outsize==1`, is_fresh `*out` 1 byte / `out` / `bp` / `outsize`),
`is_fresh(in,inend)` with `inend>=1`, `assigns(*bp,*outsize,*out,(*out)[*outsize-1])`
(realloc'd buffer writes hit freshly-allocated memory → implicitly assignable), and
the true functional postcondition `ensures(*bp==0)` (every chunk resets `*bp=0` and the
appends never touch it). Spec is strong (kills at depth≥1000); left in place.

**Loop contracts are INERT under this pipeline (confirmed 2026-06-27, via ZSTC).** The fixed
pipeline never passes `--apply-loop-contracts`, so source `__CPROVER_loop_invariant` clauses are
silently ignored — proved by inserting `__CPROVER_loop_invariant(0 == 1)` (false) into a loop and
still getting VERIFICATION SUCCESSFUL. The loop is always unwound 5×, so loop contracts CANNOT
summarize a loop to make post-loop / postcondition checks reachable under depth 200. Whatever
[[zic-dfcc-loopcontracts-12of16]] notes about "dfcc + loop contracts" does NOT apply to the avocado
scorer. Do not add loop invariants to rescue a depth-200-vacuous function — pure no-op.

**Why / how to apply:** Before chasing a higher kill score, check whether the
function's checks are even reachable under depth 200 — apply a deliberately-false
`ensures` and an OOB body mutant and binary-search `--depth`. If the false `ensures`
passes at 200 but fails at higher depth, the spec is depth-limited; 0 kills is the
ceiling and no contract rewrite will help. Don't hardcode a different depth (forbidden
by [[CLAUDE.md rules]]); leave the strongest behavior-capturing contract and document.
`Syzygy_Zopfli/c_code/test_ebl_mutants.sh` runs the per-mutant kill test via verify.sh.
