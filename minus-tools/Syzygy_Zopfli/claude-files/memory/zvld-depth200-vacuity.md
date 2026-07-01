---
name: zvld-depth200-vacuity
description: "ZopfliVerifyLenDist verifies @depth200 but 0/19 kills; first assert unreachable under 200 (irreducible dfcc overhead), strong @depth1000"
metadata: 
  node_type: memory
  type: project
  originSessionId: 2ab930a9-7e9a-4ccc-90a3-01e0b09c80f3
---

ZopfliVerifyLenDist (zopfli.c ~L3309) verifies @depth200 with full contract
(is_fresh + dist>=1 + pos>=dist + pos+length<=datasize + forall back-ref equality)
but scores **0/19 kills** at the grader's hardcoded `cbmc --depth 200`.

**Why:** Pure [[avocado-depth200-vacuity]] case. The dfcc contract-enforcement
machinery costs >200 depth steps before even the *first* assert (`assert(pos+length<=datasize)`,
the very first statement, entailed by the precondition) is reachable. At depth 1000
all 19 mutants are killed (spec is strong/exact).

**How to apply:** Don't chase. Levers tried and their thresholds for reaching the
pre-loop assert:
- is_fresh + full spec: ~300
- r_ok instead of is_fresh: ~245 (best, still >200)
- removing the loop contract (loop_invariant/assigns): no help — overhead isn't the loop machinery
- removing forall: WORSE (>300) — the assumptions actually prune paths and lower the threshold
- bare spec (only r_ok + pos+length<=datasize): still survives @200/@150/@100
The ~245 floor is irreducible dfcc overhead for this function's preconditions; no
spec restructuring gets a kill under 200. Keep is_fresh (stronger than r_ok; neither
helps @200). Scripts: /app/_verify_zvld.sh, /app/kill_zvld.sh.
