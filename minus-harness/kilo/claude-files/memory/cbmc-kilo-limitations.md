---
name: cbmc-kilo-limitations
description: CBMC contract-verification limitations hit while specifying kilo/kilo.c (avocado harness)
metadata: 
  node_type: memory
  type: project
  originSessionId: 13079476-f8c0-4468-9a7b-ddb22e547924
---

Verifying `kilo/kilo.c` with `run-cbmc` (CBMC enforce-contract + mutation testing). Limitations discovered (as of 2026-06):

- **Direct `free()` or `read()` in a function body trips a goto-instrument invariant** ("no definite size for lvalue target: symbol type bool" in `__CPROVER_deallocate` / `<builtin-library-read>`). Any function directly calling `free`/`read` under `--enforce-contract` fails to instrument. `realloc` is fine. Affected: abFree, editorFreeRow, editorUpdateRow, editorOpen, editorSave, getCursorPosition, editorReadKey. Their contracts are still usable for *replacement* in callers.
- **`--enforce-contract` havocs ALL globals** (per docs), so functions reading constant global tables (HLDB, C_HL_extensions/keywords) get nondeterministic table contents → deref failures. Affects editorSelectSyntaxHighlight, editorUpdateSyntax.
- **`__CPROVER_old(ptr->member)` returns the POST-state value, not pre-state** — useless for "value changed by N" postconditions through a pointer param. `__CPROVER_old(*ptr).member` also post-state. Workaround: express invariants using post-state only (e.g. appended region is `[size-len, size)`).
- **Memory-content postconditions (forall over arrays / pointer derefs) do NOT catch body mutations** in this setup — a mutant writing to the wrong address still "verifies". Only scalar / `__CPROVER_return_value` ensures reliably kill mutants. So high kill scores are only achievable on pure/scalar functions.
- `errno` in an assigns clause is rejected ("side-effects not allowed" — it expands to `*__errno_location()`, a call). No clean way to make errno-writes assignable → enableRawMode can't fully verify.
- Stubs live in `/app/stubs/*.c`, each modeled symbol tagged `/* FUNCTION: name */`. Created system_stubs.c (tcsetattr, tcgetattr, isatty, atexit, ioctl, signal, perror, time, strstr). Pulled in automatically when a verified function's external-callee list (from the callgraph) names them.
- Data bounds (e.g. sizes ≤ 256, numrows ≤ 8) are needed for tractability; `forall` bound of 4096 caused OOM — keep small. These are data bounds, allowed (only CBMC-arg hardcoding like unwind N is forbidden).
- is_fresh-based callee contracts don't compose when callers pass interior/global/local addresses (e.g. `&E.row[at]`, `&E.screenrows`) — is_fresh asserts a separate fresh object.
