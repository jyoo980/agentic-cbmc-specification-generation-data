---
name: zgds-equivalent-mutant
description: ZopfliGetDistSymbol exact-value contract maxes at 9/10; the dist<=5 survivor is an equivalent mutant
metadata: 
  node_type: memory
  type: project
  originSessionId: a7095cd8-1b42-4478-9060-8ca2ca91e01c
---

`ZopfliGetDistSymbol` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies with an
exact-value contract (requires dist in [1,32768]; ensures dist<5 ==> ret==dist-1;
ensures dist>=5 ==> ret == 2*l + ((dist-1)/2^(l-1) - 2) with l=31^clz(dist-1);
plus tight range and global [0,29]). Kill score = 9/10.

The lone survivor `if (dist < 5)` -> `if (dist <= 5)` is an **equivalent mutant**:
the two branches differ only at dist==5, where both yield 4 (else-branch: l=2,
r=0, 2*2+0=4; first branch: 5-1=4). No spec can kill it. 9/10 is the max — don't
chase it. Cf. [[zgdebv-equivalent-mutants]].
