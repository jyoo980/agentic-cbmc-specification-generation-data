/* No-op stub for editorUpdateRow, used only when proving editorInsertRow.
 *
 * The real editorUpdateRow ends in a (recursive) call to editorUpdateSyntax,
 * which crashes CBMC's contract inliner, and its own contract requires a fresh
 * hl buffer -- but editorInsertRow always sets row->hl = NULL before the call,
 * so editorUpdateRow's contract cannot be used here via replace-call-with-contract.
 * Its effect (recomputing render/rsize/hl of the just-inserted row) is irrelevant
 * to every editorInsertRow mutant, all of which live in the range guard, the
 * realloc/memmove shift and the malloc/memcpy of the new row's chars.
 *
 * The stub therefore does no work EXCEPT dereference its argument: this makes a
 * mutated call argument (e.g. editorUpdateRow(E.row-at) instead of E.row+at)
 * that points outside the row array a verification failure under --pointer-check.
 * The erow layout is copied verbatim from kilo.c so the field offset is exact. */
typedef struct erow {
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
    int hl_oc;
} erow;

void editorUpdateRow(erow *row)
{
    /* Force an in-bounds read of the passed-in row so a corrupted pointer is caught. */
    (void)row->size;
}
