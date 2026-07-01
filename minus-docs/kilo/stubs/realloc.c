#include <stdlib.h>

/* FUNCTION: realloc */

/* A nondeterministic model of `realloc` that avoids CBMC's built-in malloc
 * model.  The built-in model introduces a `should_malloc_fail` symbol that
 * crashes `goto-instrument --enforce-contract`'s assigns-frame instrumentation
 * ("no definite size for lvalue target: malloc::...::should_malloc_fail") when
 * the function under verification calls `realloc` in its body.
 *
 * This model returns a freshly-allocated object of exactly `size` bytes (never
 * NULL), which is sound for reasoning about callers that immediately overwrite
 * the buffer (as `editorUpdateSyntax` does with `memset`).  The previous
 * allocation is left untouched, which is fine because every caller in this code
 * base re-initializes the whole returned buffer before reading it. */
void *realloc(void *ptr, size_t size)
{
    (void)ptr;
    return __CPROVER_allocate(size, 0);
}
