---
name: kilo-disablerawmode-spec
description: disableRawMode verifies; assigns(E.rawmode)+ensures(rawmode==0); kill N/A no mutable operators
metadata: 
  node_type: memory
  type: project
  originSessionId: fe863851-8483-45d8-8b32-1d1b21536402
---

`disableRawMode(int fd)` in /app/kilo/kilo.c verifies successfully. Body is just
`if (E.rawmode) { tcsetattr(...); E.rawmode = 0; }`.

Spec: `__CPROVER_assigns(E.rawmode)` (tcsetattr stub only reads `*termios_p`,
empty assigns frame) + `__CPROVER_ensures(E.rawmode == 0)` (off-on-return both
paths). Mutation testing N/A — "no mutable operators". Related: [[kilo-enableRawMode-spec]].
