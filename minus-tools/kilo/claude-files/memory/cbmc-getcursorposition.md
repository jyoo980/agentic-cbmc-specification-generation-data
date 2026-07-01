---
name: cbmc-getcursorposition
description: getCursorPosition verifies but canonical depth-200 kill score is structurally 0 (no return reachable; global-init eats the whole budget before the first return)
metadata: 
  node_type: memory
  type: project
  originSessionId: 4fa2915a-0bac-41a7-acb6-0adf6e066de4
---

`getCursorPosition(ifd, ofd, *rows, *cols)` in kilo.c (antirez kilo) verifies but
its canonical kill score is **structurally 0/15** — no spec can score higher.

**Why 0 is the ceiling:** even `__CPROVER_ensures(0 == 1)` VERIFIES for the
original at the canonical `--depth 200`. That proves **no return point is
reachable within the depth budget** — nondeterministic global initialization of
kilo.c's large statics (HLDB / C_HL_keywords / E) exhausts ~all 200 steps before
the function's *first* `return` (the write() error path). Every postcondition is
therefore vacuous, so no mutant can ever violate one. Same family as
[[cbmc-editorreadkey]] / [[cbmc-editorsave]] / [[cbmc-editorrefreshscreen]].
Probe: `__CPROVER_ensures(rk_idx == 0)` and `rk_idx <= 1` also both verify ⇒ the
read loop's first iteration isn't even reached at a return.

**Stub linking is automatic** (tools/util/stubs.py): the canonical harness reads
the call graph's `external` callees and links the stub whose `/* FUNCTION: name */`
marker matches. getCursorPosition's externals are {read, sscanf, write}; `read`
→ stubs/readkey.c gets auto-linked, so the spec MUST `__CPROVER_requires(rk_idx
== 0)` (else the scripted reader indexes rk_ret/rk_byte OOB) and
`__CPROVER_assigns(rk_idx)` (the inlined read body writes it). write/sscanf have
no stub ⇒ nondet.

**Tried and rejected — a write() stub.** Modeling write() to expose a global
`gcp_write_ret` lets a SOUND ensures `ret==-1 ==> (gcp_write_ret!=4 || rk_idx>=1)`
kill the `!=4`→`==4` mutant *in principle*, but it still scores 0 because the
write-check return is itself beyond depth 200 (mutant probe: both `ret==0` and
`ret==-1` verify ⇒ vacuous). Net zero benefit, and `write` is a shared external
callee of editorRefreshScreen/editorSave/getWindowSize, so a stubs/write.c would
perturb them — not worth it. Removed it; left write nondet.

**Final spec (sound, verifies):** requires rk_idx==0 + is_fresh(rows)+is_fresh(cols);
assigns(rk_idx); ensures ret∈{-1,0}; ensures `rk_idx==0 ==> ret==-1` (no read
issued ⇒ took the write error path). Scoring scripts kept: kilo/_run_gcp.py
(run_cbmc wrapper), kilo/_score_gcp.py (parses get-mutants, scores).
