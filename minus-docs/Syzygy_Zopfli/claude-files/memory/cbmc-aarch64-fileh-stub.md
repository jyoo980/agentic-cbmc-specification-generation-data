---
name: cbmc-aarch64-fileh-stub
description: zopfli.c hard-codes an x86_64 FILE.h include; pass -I /tmp/cbmc-inc on this aarch64 host
metadata: 
  node_type: memory
  type: project
  originSessionId: 30f42bc6-3f06-4c1c-b5f4-9fa318a6bb67
---

`Syzygy_Zopfli/c_code/zopfli.c` line 7 hard-codes `#include <x86_64-linux-gnu/bits/types/FILE.h>`, but this host is aarch64 (only `/usr/include/aarch64-linux-gnu/...` exists). goto-cc fails to find the header.

Fix (no C change): a stub dir already exists at `/tmp/cbmc-inc/x86_64-linux-gnu/bits/types/FILE.h`. Always pass `-I /tmp/cbmc-inc` to `run-cbmc` when verifying functions in this file. If it's gone, recreate it pointing the x86_64 path at the aarch64 FILE.h. See [[cbmc-depth200-isfresh-vacuity]].
