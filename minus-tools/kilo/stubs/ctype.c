/* CBMC stub for the ctype classification machinery used by <ctype.h>.
 *
 * glibc implements isspace(c) (and the other isXXX macros) as
 *
 *     ((*__ctype_b_loc ())[(int) (c)] & (unsigned short int) _ISspace)
 *
 * so the only external symbol the macro expansion actually calls is
 * __ctype_b_loc().  CBMC has no body for it, which leaves the table lookup as
 * an unresolved nondeterministic pointer dereference.
 *
 * This stub models __ctype_b_loc() as returning a pointer into a single,
 * program-lifetime classification table.  The table contents are left
 * nondeterministic (an uninitialised non-const global, which CBMC havocs once
 * at startup), but they are *fixed* for the whole proof: every call to
 * __ctype_b_loc() hands back the same pointer to the same table.  That is the
 * key property a contract needs -- evaluating isspace(c) in a function body and
 * again in that function's `ensures` clause reads the identical table entry, so
 * the two agree, while CBMC is still free to choose any classification (e.g. a
 * table in which ' ' is a space and 'a' is not) when searching for a mutant.
 *
 * glibc's table is indexed by (int)c for c in [-128, 255] via a pointer that
 * has been biased 128 entries into a 384-entry array, so negative `char`
 * values index the low end.  We reproduce that layout.
 */

/* FUNCTION: isspace */

#define __CPROVER_CTYPE_NINDEX 384
#define __CPROVER_CTYPE_BIAS 128

static unsigned short int __CPROVER_ctype_b_table[__CPROVER_CTYPE_NINDEX];

const unsigned short int **__ctype_b_loc(void)
{
    /* Assign on every call rather than via a static initialiser: when CBMC's
     * entry point is the function under test (`--function`), static/global
     * initialisers are not run, so an initialiser here would leave `biased`
     * nondeterministic (and thus a possibly-NULL/invalid pointer).  The table
     * object itself always has a valid address; only its *contents* stay
     * nondeterministic, which is exactly what we want. */
    static const unsigned short int *biased;
    biased = &__CPROVER_ctype_b_table[__CPROVER_CTYPE_BIAS];
    return &biased;
}
