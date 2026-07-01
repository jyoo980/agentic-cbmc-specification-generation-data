---
name: kilo-editorfind-variadic-block
description: "editorFind can't be verified — goto-instrument crashes replacing the variadic editorSetStatusMessage call that passes a vararg"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5f97e702-e131-4574-b10c-f5a39a6fbe1f
---

editorFind (kilo/kilo.c) cannot be discharged by the avocado pipeline. Root cause is a goto-instrument limitation, NOT a spec weakness.

editorFind calls the variadic `editorSetStatusMessage("Search: %s ...", query)` — passing an actual vararg. The pipeline replaces every in-file callee via `--replace-call-with-contract`, and goto-instrument aborts replacing a variadic call when the callee has a NON-EMPTY assigns clause and is passed varargs:
`util/std_expr.cpp:423 function instantiate: variables.size() == values.size()`.

Minimal repro: variadic `f(const char*, ...)` with `__CPROVER_assigns(g)`, called as `f("%s", x)`. Empty assigns sidesteps it (but that's unsound for editorSetStatusMessage, which writes E.statusmsg). No contract-level workaround on editorFind's side.

**Why:** Spent a long time chasing red herrings. The visible error is `__CPROVER_replace_ensures_is_fresh: malloc not declared` + the instantiate abort, which looks like the is_fresh-vs-malloc-stub issue from [[kilo-editorRefreshScreen-spec]]. It is NOT. That warning is benign (editorRefreshScreen shows it too and passes). The real trigger is the vararg actual.

**How to apply:** For any kilo caller of editorSetStatusMessage (the only variadic fn) that passes a `%s`-style vararg, expect this crash and don't try to fix it via contracts. Diagnose by isolating which single `--replace-call-with-contract <callee>` flips the failure from `create_car_expr` to `instantiate`. Left editorFind with a strong documented contract (requires E.numrows==0 like editorRefreshScreen, assigns E.cx/cy/coloff/rowoff/statusmsg/statusmsg_time) that won't verify until goto-instrument supports variadic contract replacement. abAppend/editorSetStatusMessage were briefly edited then reverted to original; both still verify.
