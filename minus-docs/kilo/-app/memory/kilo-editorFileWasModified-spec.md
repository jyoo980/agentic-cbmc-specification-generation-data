---
name: kilo-editorfilewasmodified-spec
description: editorFileWasModified getter verifies; assigns()+return==E.dirty; kill N/A (no mutable operators)
metadata: 
  node_type: memory
  type: project
  originSessionId: 7957d04a-1a5a-40d7-9f4f-90189fb1958c
---

`editorFileWasModified` in /app/kilo/kilo.c is a trivial getter (`return E.dirty;`). Spec: `__CPROVER_assigns()` (pure observer) + `__CPROVER_ensures(__CPROVER_return_value == E.dirty)` (strongest possible). Verifies directly; mutation testing N/A — "no mutable operators", same as [[kilo-editorSetStatusMessage-spec]] and [[kilo-initEditor-spec]].
