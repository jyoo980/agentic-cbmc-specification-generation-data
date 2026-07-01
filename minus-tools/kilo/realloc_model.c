/* realloc model used only when proving editorInsertRow.
 *
 * CBMC's default realloc havocs the contents of the returned object and does
 * not register that object as assignable within the caller's frame, which makes
 * every write into the grown E.row array (E.row[at].* and the idx++ shift) fail
 * the assigns check and leaves the preserved row indices nondeterministic.
 *
 * This faithful model instead:
 *   - mallocs a fresh object of the requested size.  Because the allocation
 *     happens inside the function under analysis, the object is implicitly
 *     assignable, so the subsequent E.row[...] writes are in-frame.
 *   - copies the preserved prefix (min(old,new) bytes) from the old object, so
 *     the existing rows' idx/size/pointers survive the reallocation exactly as
 *     real realloc guarantees.
 *   - frees the old object (matching editorInsertRow's `__CPROVER_frees(E.row)`).
 * With --no-malloc-may-fail the malloc never returns NULL. */
#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);

/* memmove model: CBMC does not auto-model memmove here, so the shift of the row
 * array would otherwise be a nondeterministic no-op (leaving moved rows' idx
 * unconstrained).  Implemented via a fresh temporary so overlapping ranges are
 * handled correctly, and using the built-in memcpy so it stays unwind-free. */
void *memmove(void *dest, const void *src, size_t n)
{
    if (n == 0)
        return dest;
    void *tmp = malloc(n);
    memcpy(tmp, src, n);
    memcpy(dest, tmp, n);
    return dest;
}

void *realloc(void *ptr, size_t size)
{
    void *p = malloc(size);
    if (p == (void *)0)
        return (void *)0;
    if (ptr != (void *)0) {
        size_t old = __CPROVER_OBJECT_SIZE(ptr);
        size_t n = old < size ? old : size;
        memcpy(p, ptr, n);
        free(ptr);
    }
    return p;
}
