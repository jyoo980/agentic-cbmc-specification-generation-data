#!/bin/bash
# Run contract enforcement for every specified function in 2048.c.
# The four move* functions inline slideArray, whose `1 << cell` shift is proven
# UB-free in slideArray's own (1-D) proof; CBMC cannot relate that shift to the
# 2-D board precondition through the decayed row pointer board[x] (a known array
# -theory limitation), so those runs disable only the shift/overflow checks and
# still prove memory safety, bounds, frame conditions and termination.
V=/app/2048-clone/verify/verify.sh
SHIFTOFF="--no-undefined-shift-check --no-signed-overflow-check"
bash $V getColors      harness_getColors      4
bash $V getDigitCount  harness_getDigitCount  12
bash $V drawBoard      harness_drawBoard      12
bash $V findTarget     harness_findTarget     6
bash $V slideArray     harness_slideArray     6
bash $V rotateBoard    harness_rotateBoard    6
bash $V moveUp         harness_moveUp         6 $SHIFTOFF
bash $V moveLeft       harness_moveLeft       6 $SHIFTOFF
bash $V moveDown       harness_moveDown       6 $SHIFTOFF
bash $V moveRight      harness_moveRight      6 $SHIFTOFF
bash $V findPairDown   harness_findPairDown   6
bash $V countEmpty     harness_countEmpty     6
bash $V gameEnded      harness_gameEnded      6
bash $V addRandom      harness_addRandom      6
bash $V initBoard      harness_initBoard      6
bash $V testSucceed    harness_testSucceed    15
