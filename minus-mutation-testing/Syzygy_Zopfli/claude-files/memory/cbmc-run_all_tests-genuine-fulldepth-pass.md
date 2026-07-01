---
name: cbmc-run_all_tests-genuine-fulldepth-pass
description: "zopfli run_all_tests verifies GENUINELY (non-vacuous) at full depth, unlike its vacuous callee single_test"
metadata: 
  node_type: memory
  type: project
  originSessionId: d9b80101-167d-4916-bf6d-121043e8cba8
---

In /app/Syzygy_Zopfli/c_code/zopfli.c, `run_all_tests` verifies genuinely — confirmed non-vacuous at FULL depth, not just avocado's --depth 200.

Spec: is_fresh(in,101) + in[100]==0 + in[ANCB_MAX_INPUT]==0, assigns(errno). It forwards `in` to single_test with btype 0/1/2; the btype-0 call needs in[8]==0, which is why in[ANCB_MAX_INPUT]==0 is load-bearing.

Verified via probe: removing in[ANCB_MAX_INPUT]==0 still passes under avocado (depth 200 slices the btype-0 call-site assertion as unreachable) but FAILS single_test.precondition.9 at line 6375 under full-depth `cbmc checking-run_all_tests-contracts.goto`. With the clause restored: full-depth = "0 of 24853 failed, VERIFICATION SUCCESSFUL" (all 27 call-site precondition checks pass).

Contrast with [[cbmc-single_test-vacuous-out-null-mismatch]]: run_all_tests uses single_test's CONTRACT (not body), so it does not inherit single_test's own vacuous ZopfliDeflate problem. The depth-200 slicing here only hid a precondition check during probing; the real spec passes soundly at full depth.
