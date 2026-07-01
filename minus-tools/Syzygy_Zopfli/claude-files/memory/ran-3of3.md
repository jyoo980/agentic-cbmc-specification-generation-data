---
name: ran-3of3
description: "Ran (Marsaglia MWC RNG) verifies @depth200 with exact-value postcond, kills all 3 mutants"
metadata: 
  node_type: memory
  type: project
  originSessionId: a593c24f-6720-4428-8133-9641e669f5cf
---

`Ran(RanState*)` in /app/Syzygy_Zopfli/c_code/zopfli.c — leaf MWC RNG, no loops.
3 avocado mutants, all `+`→`-` (on m_z update, m_w update, and return). All
killed 3/3 @depth200 with exact-value postcondition:
- is_fresh(state) + assigns(*state)
- ensures m_z == 36969*(__CPROVER_old(m_z)&65535) + (old_m_z>>16)
- ensures m_w == 18000*(__CPROVER_old(m_w)&65535) + (old_m_w>>16)
- ensures return == (unsigned)((m_z<<16) + m_w)  [refers to NEW state]

All unsigned int → wrap mod 2^32 matches contract exprs. Same leaf-closed-form
recipe as [[uhv-no-mutants]] / [[irs-no-mutants]] but here there ARE mutants and
they all die. Scripts: /app/_verify_ran.sh, /app/kill_ran.sh.
