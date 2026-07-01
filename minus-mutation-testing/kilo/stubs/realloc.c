/* CBMC stub model for realloc.
 *
 * goto-cc's automatically linked CPROVER library models malloc/free/memcpy/memmove,
 * but it does NOT provide a model for realloc: an unmodeled realloc is treated as a
 * fully nondeterministic function that may return NULL or a pointer to an
 * arbitrarily-sized object, which makes any subsequent access to the reallocated
 * buffer impossible to verify.  The CBMC-bundled realloc model cannot be injected via
 * `goto-instrument --add-library` either: doing so makes the assigns-clause
 * instrumentation abort with an invariant violation on the malloc model's internal
 * `should_malloc_fail` symbol.
 *
 * This stub supplies a sound, self-contained model: realloc returns a freshly
 * allocated object of `size` bytes (so the result is a valid, distinct, writable
 * region tracked as locally allocated by the caller under verification) and releases
 * the old allocation.  The bytes of the new object are left nondeterministic, which is
 * an over-approximation of realloc's "copy the previous contents" behaviour and hence
 * sound for memory-safety reasoning.
 */

/* FUNCTION: realloc */

#include <stdlib.h>

void *realloc(void *ptr, size_t size) {
    /* A fresh, distinct, writable object of exactly `size` bytes. */
    void *result = __CPROVER_allocate(size, 0);
    /* realloc(ptr, size) releases the storage previously pointed to by ptr. */
    if (ptr != 0) {
        __CPROVER_deallocate(ptr);
    }
    return result;
}
