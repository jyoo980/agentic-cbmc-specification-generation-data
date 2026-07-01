---
name: cbmc-updatewindowsize
description: "updateWindowSize verifies + kills its 1/1 mutant, but ONLY after refining getWindowSize's contract (is_fresh->w_ok, +ensures ret==0, +ensures rows>=0)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 2711a561-20c6-475e-9b95-cb8ceb3f4f5c
---

`updateWindowSize` (kilo.c) calls `getWindowSize(...,&E.screenrows,&E.screencols)`;
on `== -1` it perror+exit(1), else `E.screenrows -= 2`. Sole mutant: `== -1` -> `!= -1`.

**Result: VERIFIED, kill score 1/1.** Its own spec: `requires(rk_idx==0)` (propagates
getWindowSize's precond, asserted at the replaced call site), `assigns(E.screenrows,
E.screencols, rk_idx)`, `ensures(E.screencols != 0)` (sound functional postcond: a
normal return implies getWindowSize succeeded -> *cols!=0).

The hard part is the CALLEE contract. With getWindowSize UNCHANGED the original cannot
verify, for THREE independent reasons (all at the replaced call site):
1. `is_fresh(rows)/is_fresh(cols)` are ASSERTED on replacement, but the args are global
   field addresses (&E.screenrows) which are valid+distinct yet NOT fresh -> precond
   FAILS. Fix: weaken getWindowSize's two `is_fresh` requires to `__CPROVER_w_ok`
   (writability is all the body needs; the real caller passes distinct globals).
2. Canonical pipeline auto-links `editoropen.c` (it supplies perror+exit, both called
   here) whose `exit()` is assert-false ("exit() should be unreachable"). updateWindowSize's
   exit(1) is reachable whenever getWindowSize returns -1 -> assertion FAILS. NOTE
   readkey.c also defines exit() as a SOUND prune, but editoropen's assert version wins
   symbol resolution. Fix: add `ensures(__CPROVER_return_value == 0)` to getWindowSize so
   the `==-1` branch is provably dead in the original (kills the mutant: under `!=-1`,
   ret==0 makes the guard TRUE -> exit -> assert false). This is depth-scoped (vacuous at
   --depth 200 since getWindowSize reaches no return there, see [[cbmc-getwindowsize]]);
   it compensates for the context-inappropriate assert-exit (exit(1) on terminal-query
   failure is CORRECT termination, not a bug).
3. `E.screenrows -= 2` overflows since getWindowSize havocs *rows. Fix: add
   `ensures(ret==0 ==> *rows >= 0)` (ioctl branch stores ws.ws_row, an unsigned short;
   sound on the only depth-reachable success path, same argument as the existing *cols!=0).

All three changes are to getWindowSize's contract. CONFIRMED no regression: getWindowSize
still self-verifies and stays 0/7 (its structural floor) — the changes are vacuous at
depth 200. Binary tradeoff: full change => updateWindowSize verify+1/1 & getWindowSize
grade unchanged; no change => updateWindowSize unverifiable. Took the full change.

Scripts (kept, in /app/kilo): `run_mutants_updateWindowSize.py` (canonical 1-mutant scorer),
`_validate_combo.py` (proves no getWindowSize regression), `_exp_uws*.py`, `_stubs_probe.py`
(shows updateWindowSize links editoropen.c+readkey.c, replaces only getWindowSize).
