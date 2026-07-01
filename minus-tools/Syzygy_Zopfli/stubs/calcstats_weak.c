#include "zopfli.h"
#include <math.h>

/* Weak, sound over-approximation of ZopfliCalculateEntropy for verifying its
   caller CalculateStatistics modularly.

   The real contract pins n == ZOPFLI_NUM_LL (288) and uses is_fresh on its two
   pointer parameters.  Neither works at CalculateStatistics's call sites: the
   second call passes n == ZOPFLI_NUM_D (32), and both calls pass *struct fields*
   (stats->litlens / stats->ll_symbols, stats->dists / stats->d_symbols) which
   are not separately-allocated objects, so is_fresh would fail on them.

   This weak model keeps only the part the caller depends on: for any positive n,
   given a readable count buffer and a writable bitlengths buffer, it may rewrite
   the whole bitlengths array and guarantees the first entry is non-negative
   (every entry is a clamped, non-negative entropy contribution).  r_ok / w_ok
   are used instead of is_fresh so the contract can be replaced at a call site
   whose arguments are struct fields. */
void ZopfliCalculateEntropy(const size_t *count, size_t n, double *bitlengths)
__CPROVER_requires(n > 0)
__CPROVER_requires(__CPROVER_r_ok(count, n * sizeof(*count)))
__CPROVER_requires(__CPROVER_w_ok(bitlengths, n * sizeof(*bitlengths)))
__CPROVER_assigns(__CPROVER_object_upto(bitlengths, n * sizeof(*bitlengths)))
__CPROVER_ensures(bitlengths[0] >= 0)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        double v;
        __CPROVER_assume(v >= 0);
        bitlengths[i] = v;
    }
}
