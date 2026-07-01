/* CBMC stub model for free.
 *
 * goto-cc's automatically linked CPROVER library models free, but its model
 * introduces an internal nondeterministic `bool` temporary that the
 * assigns/frees-clause instrumentation cannot size: when a function carrying a
 * `__CPROVER_frees(...)` contract calls free() in its body, goto-instrument
 * aborts with
 *
 *     Invariant check failed ... create_car_expr
 *     Reason: no definite size for lvalue target ... __CPROVER_deallocate
 *
 * This stub supplies a sound, self-contained model that defers directly to the
 * `__CPROVER_deallocate` primitive (the same primitive the realloc stub uses),
 * sidestepping the bundled model's problematic temporary.  free(NULL) is a
 * no-op, matching the C standard.
 */

/* FUNCTION: free */

#include <stdlib.h>

void free(void *ptr) {
    if (ptr != 0) {
        __CPROVER_deallocate(ptr);
    }
}
