#include "zopfli.h"

/* Weak, sound over-approximation of ZopfliStoreLitLenDist for verifying its
   callers modularly: it appends exactly one command (bumping target->size by
   one) and may arbitrarily rewrite the eight parallel buffers.  This is the
   part of the real contract that callers depend on; unlike the real contract
   it requires no `is_fresh`/non-power-of-two regime, so it can be replaced at
   a call site that fires repeatedly inside a loop. */
void ZopfliStoreLitLenDist(unsigned short length, unsigned short dist,
                           size_t pos, ZopfliLZ77Store *target)
__CPROVER_assigns(target->size)
__CPROVER_assigns(__CPROVER_object_whole(target->litlens))
__CPROVER_assigns(__CPROVER_object_whole(target->dists))
__CPROVER_assigns(__CPROVER_object_whole(target->pos))
__CPROVER_assigns(__CPROVER_object_whole(target->ll_symbol))
__CPROVER_assigns(__CPROVER_object_whole(target->d_symbol))
__CPROVER_assigns(__CPROVER_object_whole(target->ll_counts))
__CPROVER_assigns(__CPROVER_object_whole(target->d_counts))
__CPROVER_ensures(target->size == __CPROVER_old(target->size) + 1)
{
    target->size++;
}
