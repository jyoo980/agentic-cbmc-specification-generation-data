/* Nondeterministic stub for editorInsertRow, used only by the editorOpen
 * mutation-measurement harness (linked after --remove-function-body so the real
 * body is gone).
 *
 * editorOpen's loop calls editorInsertRow(E.numrows, line, linelen) once per line
 * read.  Replacing the call with editorInsertRow's real contract is unusable here:
 * that contract pins E.numrows == 2, but the loop calls it repeatedly, so the
 * second iteration would violate the precondition.  Inlining the real body drags
 * in realloc/editorUpdateRow/editorUpdateSyntax.  Neither is needed: every line
 * editorOpen produces flows through (s, len), and the only editorOpen mutants that
 * touch that argument change *which* bytes/length are handed over.
 *
 * The stub therefore performs no state update (editorOpen resets E.dirty to 0 on
 * exit regardless) but asserts the three properties a correctly-extracted line
 * must have -- non-empty, a readable len+1-byte buffer, and a stripped trailing
 * newline.  A mutant that mis-handles the newline test or the length delivers a
 * line that violates one of these, and is killed. */

#include <stddef.h>

void editorInsertRow(int at, char *s, size_t len)
{
    (void)at;
    __CPROVER_precondition(len >= 1, "editorInsertRow receives a non-empty line");
    __CPROVER_precondition(__CPROVER_r_ok(s, len + 1), "editorInsertRow line buffer readable");
    __CPROVER_precondition(s[len - 1] != '\n' && s[len - 1] != '\r',
                           "editorInsertRow line has its trailing newline stripped");
}
