---
name: zgdebv-equivalent-mutants
description: ZopfliGetDistExtraBitsValue maxes at 8/10 kills; two survivors are equivalent mutants
metadata: 
  node_type: memory
  type: project
  originSessionId: e8e4994c-2cb1-49de-a980-c637f1a5f42f
---

In `Syzygy_Zopfli/c_code/zopfli.c`, `ZopfliGetDistExtraBitsValue` verifies with an exact-value contract (requires dist in [1,32768]; ensures dist<5 ⇒ 0, else return == (dist-1) & ((1<<(l-1))-1) with l = 31^clz(dist-1), plus a tight [0,2^(l-1)) range). Kill score is 8/10 and that is the MAX.

The two surviving mutants are provably equivalent — do not chase them:
- `dist < 5` → `dist <= 5`: differs only at dist==5, where the else branch also yields 0.
- `(dist - (1 - (1<<l)))` (i.e. +2^l) vs original `- (1<<l)`: result is masked by `(1<<(l-1))-1`; ±2^l preserves residue mod 2^(l-1), so the low l-1 bits are identical.

Sibling [[adddynamictree-out-zero-regime]] pattern: some 0-kill/residual-survivor regimes are inherent. Killscore script: `killscore_zgdebv.py` (run with `PYTHONSAFEPATH=1` to avoid local `bisect.py` shadowing stdlib).
