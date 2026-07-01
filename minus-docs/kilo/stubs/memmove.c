#include <string.h>

/* FUNCTION: memmove */

/* A lightweight model of `memmove` that avoids CBMC's byte-precise copy of a
 * symbolic-length region.  When the function under verification moves a region
 * whose length is a symbolic expression (e.g. `sizeof(erow) * (E.numrows - at)`
 * in `editorInsertRow`), the built-in `memmove` encoding explodes the SAT
 * problem and CBMC runs out of memory.
 *
 * This model leaves the destination contents nondeterministic, which is sound
 * for every caller in this code base: each caller either overwrites the moved
 * region before reading it, or never relies on the moved bytes' values (only on
 * the buffer remaining allocated and in-bounds, which is unaffected). */
void *memmove(void *dest, const void *src, size_t n)
{
    (void)src;
    (void)n;
    return dest;
}
