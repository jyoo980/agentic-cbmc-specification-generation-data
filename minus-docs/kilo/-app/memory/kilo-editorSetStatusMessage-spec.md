---
name: kilo-editorsetstatusmessage-spec
description: "editorSetStatusMessage verifies with is_fresh(fmt,1)+assigns(statusmsg,statusmsg_time); no mutable operators so kill score N/A"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5ec9efcc-ab92-46ce-8f8a-d23493f91305
---

`editorSetStatusMessage(const char *fmt, ...)` in /app/kilo/kilo.c is a varargs
function whose body is entirely library calls (va_start, vsnprintf into
E.statusmsg[80], va_end, time(NULL)→E.statusmsg_time). CBMC handles the
varargs/vsnprintf fine.

Working spec:
```
__CPROVER_requires(__CPROVER_is_fresh(fmt, 1))
__CPROVER_assigns(E.statusmsg, E.statusmsg_time)
```
Verifies. Mutation testing reports "no mutable operators" — body has no
arithmetic/comparison operators, so kill score is not applicable (unlike the
vacuity concern in [[kilo-cbmc-contracts-vacuous]]).
