---
name: zmcs-5of5
description: "ZopfliMaxCachedSublen verifies with exact-value contract, kills 5/5 mutants"
metadata: 
  node_type: memory
  type: project
  originSessionId: 7e58bb9a-8223-4644-884e-302a5b707836
---

ZopfliMaxCachedSublen (zopfli.c) — leaf, no loops, reads cache=&lmc->sublen[ZOPFLI_CACHE_LENGTH*pos*3], returns 0 if cache[1]&&cache[2] both 0 else cache[21]+3.

Verifies + kills 5/5 via simple enforce-contract flow (no dfcc): is_fresh(lmc) + is_fresh(lmc->sublen, (24*pos+22) bytes) sized to exactly cover indices 1,2,21 of the slice + pos<=1024 bound + assigns() + exact-value ensures pinning both the && guard and the +3/index. No depth-200 vacuity since no loops. Scripts: /app/_verify_zmcs.sh, /app/kill_zmcs.sh.
