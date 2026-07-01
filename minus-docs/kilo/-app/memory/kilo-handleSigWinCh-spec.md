---
name: kilo-handlesigwinch-spec
description: "handleSigWinCh NOT dischargeable: updateWindowSize can't ensure screenrows>=0 (degenerate <2-row terminal), so editorRefreshScreen precond 4/5/6 fail. Strong contract retained."
metadata: 
  node_type: memory
  type: project
  originSessionId: d84e9b31-7f13-4789-9f32-f5e01822cb72
---

`handleSigWinCh` (kilo.c, SIGWINCH handler) calls `updateWindowSize()`, clamps
`E.cy = E.screenrows-1` / `E.cx = E.screencols-1`, then calls
[[kilo-editorRefreshScreen-spec]]. Contract: requires `numrows==0, rowoff>=0,
coloff>=0, cx>=0, cy>=0`; assigns `E.cx, E.cy, E.screenrows, E.screencols`.

**NOT dischargeable** — 3 residual failures, all the degenerate-window trio:
`editorRefreshScreen.precondition.4 (cx>=0), .5 (cy>=0), .6 (screenrows>=0)`.
Root cause: getWindowSize bounds each dim in `[0,0xffff]`, and updateWindowSize
subtracts 2 for the status bar, so `E.screenrows` can be `-2` for a 0/1-row
terminal. updateWindowSize therefore **cannot** ensure `screenrows>=0` (body
genuinely produces -2). The clamps then drive cy/cx negative. No handleSigWinCh
precondition can constrain updateWindowSize's nondet output. Only fix would be
weakening the verified shared editorRefreshScreen contract — declined.

**Side improvement (kept):** added bounded `ensures` to
[[kilo-updateWindowSize-spec]]: `-2 <= E.screenrows <= 0xffff-2` and
`0 <= E.screencols <= 0xffff`. Provable from its body; re-verified updateWindowSize
**still 1/1 kill**. These ensures discharge handleSigWinCh's two signed-`-`
overflow obligations (`screenrows-1`, `screencols-1`) and editorRefreshScreen
precond .7 (`screencols>=0`); without them screenrows/screencols are fully
havoc'd and ALL of precond 4/5/6/7 + both overflows fail. Net: reduced 6
failures → 3 irreducible ones.
