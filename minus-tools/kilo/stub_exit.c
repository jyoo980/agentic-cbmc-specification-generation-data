/* exit() stub, used only when proving editorUpdateRow.
 *
 * editorUpdateRow calls exit(1) only on the "line too long" path, guarded by
 * `allocsize > UINT32_MAX`.  Under editorUpdateRow's contract the row has
 * size == 1, so allocsize is a tiny constant and that guard is provably false:
 * the real code never terminates the process.  Modeling exit() as an assertion
 * failure (rather than CBMC's default no-return assume(false), which silently
 * prunes the path) turns "reaching exit" into a verification failure.  This
 * leaves the original proof unaffected (exit is unreachable) while killing
 * mutants that corrupt the overflow guard into firing spuriously -- without
 * such a model those mutants survive because the pruned exit path never reaches
 * the postcondition checks. */
void exit(int status)
{
    (void)status;
    __CPROVER_assert(0, "exit() is unreachable under editorUpdateRow's contract");
    __CPROVER_assume(0);
}
