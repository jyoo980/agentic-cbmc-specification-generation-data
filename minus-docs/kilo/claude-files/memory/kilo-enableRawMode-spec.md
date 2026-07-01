---
name: kilo-enablerawmode-spec
description: kilo enableRawMode verifies NON-vacuously; 0/6 kill is unavoidable (equivalent mutants on nondet-driven branch guards). Needed new errno + termios stubs.
metadata: 
  node_type: memory
  type: project
  originSessionId: 4e15385f-3f00-47b9-8eae-138bf664de3d
---

`enableRawMode` in `/app/kilo/kilo.c` VERIFIES and is **non-vacuous** (false
`__CPROVER_ensures(return==12345)` probe FAILS — there are no `requires`/`is_fresh`,
so no UNSAT poison). Kill score is **0/6 and that is unavoidable**, NOT weakness.

**Spec** (no requires; assigns `E.rawmode, orig_termios, __avocado_errno`):
fully characterizes observable post-state on all 3 paths —
- `old(rawmode)!=0` ⇒ return 0 & rawmode unchanged (early "already enabled");
- `old(rawmode)==0 && return 0` ⇒ rawmode==1 (success enables);
- `return -1` ⇒ rawmode==old & errno==ENOTTY (fatal).

**The 6 surviving mutants are EQUIVALENT mutants:** all are relational-operator
flips on the branch guards `tcgetattr(...)==-1` and `tcsetattr(...)<0`. Those
guards test the *nondeterministic* return of the terminal stubs. Any relational
mutant just changes which nondet value routes to `fatal`; both arms still reach
spec-conformant observable states (success: rawmode=1,ret 0 / fatal:
errno=ENOTTY,ret -1), both stay reachable. Unkillable without UNSOUNDLY forcing
the stubs to always succeed. The `raw` struct config (c_iflag/oflag/cflag/lflag,
VMIN/VTIME) is also unobservable — it only feeds tcsetattr, whose nondet stub
ignores its arg.

**New stubs created** (`/app/stubs/`): `isatty.c`, `atexit.c`, `tcgetattr.c`
(havocs *termios_p on success), `tcsetattr.c` (read-only). All nondet/sound.
Without bodies, avocado flags `no body for callee ...: FAILURE`.

**errno framing trick (reusable):** the body's `errno = ENOTTY` on the fatal
path MUST be in the assigns frame, but `errno` = `(*__errno_location())` and a
function call is rejected in an assigns target ("side-effects not allowed").
Also: with NO assigns clause, --enforce-contract enforces an EMPTY frame, so the
errno write fails anyway. Fix: model `__errno_location` to return `&__avocado_errno`
(a concrete global) — folded into `isatty.c` because `__errno_location` is NOT in
the call graph's external list (the `errno` macro only appears after
preprocessing) so it can't be supplied as its own auto-included stub file; it must
ride in an already-included stub. Then `extern int __avocado_errno;` in kilo.c +
`__CPROVER_assigns(..., __avocado_errno)`. See [[kilo-cbmc-contracts-vacuous]].
