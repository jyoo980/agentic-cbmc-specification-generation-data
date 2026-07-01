#include <stdlib.h>

/* FUNCTION: malloc */

/* A nondeterministic model of `malloc` that avoids CBMC's built-in malloc model.
 * The built-in model introduces a `should_malloc_fail` symbol that crashes
 * `goto-instrument --enforce-contract`'s assigns-frame instrumentation ("no
 * definite size for lvalue target: malloc::...::should_malloc_fail") when the
 * function under verification calls `malloc` in its body (as `editorOpen` does).
 * This mirrors the existing `realloc` model in this directory.
 *
 * The model returns a freshly-allocated object of exactly `size` bytes and never
 * returns NULL.  That is sound for the callers here, which assume allocation
 * succeeds and immediately initialize the returned buffer. */
void *malloc(size_t size)
{
    return __CPROVER_allocate(size, 0);
}
