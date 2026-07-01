/* CBMC stub model for malloc.
 *
 * goto-cc automatically links the CPROVER library's malloc model, but that model
 * introduces an internal nondeterministic static `should_malloc_fail` boolean.
 * When a function carrying an assigns/frees contract calls malloc() in its body,
 * the assigns-clause instrumentation tries to build a "car" (conditional address
 * range) expression for that static and aborts with
 *
 *     Invariant check failed ... create_car_expr
 *     Reason: no definite size for lvalue target ... should_malloc_fail
 *
 * This stub supplies a sound, self-contained model that defers directly to the
 * `__CPROVER_allocate` primitive (the same primitive the realloc stub uses),
 * sidestepping the bundled model's problematic temporary.  It returns a fresh,
 * distinct, writable object of exactly `size` bytes whose contents are left
 * nondeterministic, matching malloc's "uninitialised storage" behaviour.  The
 * allocation never fails (returns non-NULL), which is the convention the rest of
 * the kilo verification relies on (the code never checks malloc's result).
 */

/* FUNCTION: malloc */

#include <stdlib.h>

void *malloc(size_t size) {
    /* A fresh, distinct, writable object of exactly `size` bytes. */
    return __CPROVER_allocate(size, 0);
}
