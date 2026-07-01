#include "zopfli.h"

/* Weak, sound over-approximations of ZopfliBlockSplit's heavy/composition-
   breaking callees, used ONLY to validate (off the fixed --depth 200) that the
   ZopfliBlockSplit contract is strong, i.e. its conversion-loop mutants are
   killed once the body becomes reachable.  These are NOT part of the avocado
   scoring path (no FUNCTION: markers, not auto-discovered); the real bodies and
   contracts in zopfli.c are what avocado scores.

   The real greedy callee requires is_fresh() of the store's seven command
   arrays, which ZopfliInitLZ77Store leaves NULL, so they cannot be composed by
   replace-contract; these stubs drop the unsatisfiable preconditions while
   keeping the one fact the driver depends on for the tiny-input regime:
   store.size <= inend (the parse emits at most one command per advanced byte). */

void ZopfliInitLZ77Store(const unsigned char *data, ZopfliLZ77Store *store)
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
__CPROVER_assigns(__CPROVER_object_whole(store))
__CPROVER_ensures(store->size == 0)
__CPROVER_ensures(store->litlens == NULL)
__CPROVER_ensures(store->dists == NULL)
{
    store->size = 0;
    store->litlens = 0;
    store->dists = 0;
    store->pos = 0;
    store->data = data;
    store->ll_symbol = 0;
    store->d_symbol = 0;
    store->ll_counts = 0;
    store->d_counts = 0;
}

void ZopfliInitBlockState(const ZopfliOptions *options, size_t blockstart,
                          size_t blockend, int add_lmc, ZopfliBlockState *s)
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_assigns(__CPROVER_object_whole(s))
{
}

void ZopfliAllocHash(size_t window_size, ZopfliHash *h)
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_assigns(__CPROVER_object_whole(h))
{
}

void ZopfliLZ77Greedy(ZopfliBlockState *s, const unsigned char *in,
                      size_t instart, size_t inend,
                      ZopfliLZ77Store *store, ZopfliHash *h)
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
__CPROVER_requires(instart <= inend)
__CPROVER_assigns(store->size)
__CPROVER_ensures(store->size <= inend)
{
    size_t n;
    __CPROVER_assume(n <= inend);
    store->size = n;
}

void ZopfliCleanBlockState(ZopfliBlockState *s)
__CPROVER_assigns()
{
}

void ZopfliCleanLZ77Store(ZopfliLZ77Store *store)
__CPROVER_assigns()
{
}

void ZopfliCleanHash(ZopfliHash *h)
__CPROVER_assigns()
{
}
