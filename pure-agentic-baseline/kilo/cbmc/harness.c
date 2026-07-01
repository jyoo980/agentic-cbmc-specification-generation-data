/* DFCC proof harnesses for kilo.c functions whose contracts involve heap
 * deallocation (a `frees` clause), which the deprecated non-DFCC contract
 * enforcement path does not support.
 *
 * Including kilo.c directly (rather than linking) gives each harness access to
 * the real struct definitions and static globals without duplicating them, and
 * leaves kilo.c itself carrying nothing but its specifications.  Each harness
 * is the DFCC entry point and simply calls its target with unconstrained
 * arguments; the target's __CPROVER_requires clauses (via __CPROVER_is_fresh)
 * establish the needed memory.
 */
#include "../kilo.c"

void h_abFree(void)        { struct abuf *ab; abFree(ab); }
void h_editorFreeRow(void) { erow *row; editorFreeRow(row); }
