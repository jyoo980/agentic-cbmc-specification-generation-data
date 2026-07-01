---
name: kilo-editoratexit-spec
description: "editorAtExit just delegates to disableRawMode; assigns(E.rawmode)+ensures(rawmode==0) verifies, kill N/A"
metadata: 
  node_type: memory
  type: project
  originSessionId: 1f654a22-99fa-4ab6-afdf-091c51a2cebe
---

`editorAtExit` (kilo.c) is a thin wrapper: body is `disableRawMode(STDIN_FILENO);`. Spec mirrors callee [[kilo-disableRawMode-spec]]: `assigns(E.rawmode)` + `ensures(E.rawmode == 0)`. Verifies. Mutation testing N/A — "no mutable operators".
