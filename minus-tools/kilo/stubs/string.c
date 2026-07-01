/* CBMC stub models for the few <string.h>/<stdlib.h> functions used by
 * editorUpdateSyntax.  Without bodies, CBMC treats these as nondeterministic
 * no-ops, so their memory effects (and the bounds violations that mutants
 * introduce) are invisible.  These faithful models make the writes real and
 * assert that every accessed region is in bounds, so out-of-bounds mutants are
 * caught. */

#include <stddef.h>

void *memset(void *s, int c, size_t n)
{
    __CPROVER_precondition(__CPROVER_w_ok(s, n),
                           "memset destination region writeable");
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; i++)
        p[i] = (unsigned char)c;
    return s;
}

int memcmp(const void *a, const void *b, size_t n)
{
    __CPROVER_precondition(__CPROVER_r_ok(a, n),
                           "memcmp first region readable");
    __CPROVER_precondition(__CPROVER_r_ok(b, n),
                           "memcmp second region readable");
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i])
            return (int)x[i] - (int)y[i];
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t i = 0;
    while (s[i] != '\0')
        i++;
    return i;
}

void *realloc(void *ptr, size_t size)
{
    /* editorUpdateSyntax calls realloc(row->hl, row->rsize) where the caller's
     * hl already points to a tight, bounds-tracked is_fresh object of exactly
     * row->rsize bytes.  Modelling the reallocation in place (returning the
     * same object) preserves those bounds, so off-by-one accesses into hl are
     * still caught.  A freshly malloc'd object, by contrast, is treated as
     * having unbounded extent under contract enforcement, which would silence
     * the very checks that kill mutants. */
    (void)size;
    return ptr;
}
