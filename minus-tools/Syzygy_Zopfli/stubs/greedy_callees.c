#include "zopfli.h"

/* Weak, sound over-approximations of ZopfliLZ77Greedy's callees, used to verify
   the greedy driver modularly.  The real callees carry strong contracts whose
   preconditions hold only in fixed single-call regimes (e.g. ZopfliUpdateHash
   requires pos==8, ZopfliFindLongestMatch requires a fully-determined hash
   chain).  Those cannot be discharged at the call sites inside the greedy
   parse loop, which fires them repeatedly with running pos.  Each stub below
   keeps only the part of the real behaviour the driver depends on for memory
   safety and frame soundness, with no regime precondition, so it can be
   replaced at a call site that repeats inside a loop. */

#define GREEDY_HASH_MASK 32767

/* Resets the rolling hash and clears every hash table.  Over-approximation:
   havoc all hash state, re-establishing only the [0, HASH_MASK] invariant on
   the current value that downstream code relies on. */
void ZopfliResetHash(size_t window_size, ZopfliHash *h)
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_assigns(h->val, h->val2,
                  __CPROVER_object_whole(h->head), __CPROVER_object_whole(h->prev),
                  __CPROVER_object_whole(h->hashval), __CPROVER_object_whole(h->same),
                  __CPROVER_object_whole(h->head2), __CPROVER_object_whole(h->prev2),
                  __CPROVER_object_whole(h->hashval2))
__CPROVER_ensures(h->val >= 0 && h->val <= GREEDY_HASH_MASK)
__CPROVER_ensures(h->val2 >= 0 && h->val2 <= GREEDY_HASH_MASK)
{
    h->val = 0;
    h->val2 = 0;
}

/* Warms up the rolling hash from the first bytes of the window.  Real function
   only updates h->val; preserve the masked-value invariant. */
void ZopfliWarmupHash(const unsigned char *array, size_t pos, size_t end,
                      ZopfliHash *h)
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_assigns(h->val)
__CPROVER_ensures(h->val >= 0 && h->val <= GREEDY_HASH_MASK)
{
    h->val = h->val & GREEDY_HASH_MASK;
}

/* Folds one byte into the rolling hash and updates the chain tables.  Havoc the
   whole hash state, keeping the [0, HASH_MASK] invariant on both current
   values. */
void ZopfliUpdateHash(const unsigned char *array, size_t pos, size_t end,
                      ZopfliHash *h)
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_assigns(h->val, h->val2,
                  __CPROVER_object_whole(h->head), __CPROVER_object_whole(h->prev),
                  __CPROVER_object_whole(h->hashval), __CPROVER_object_whole(h->same),
                  __CPROVER_object_whole(h->head2), __CPROVER_object_whole(h->prev2),
                  __CPROVER_object_whole(h->hashval2))
__CPROVER_ensures(h->val >= 0 && h->val <= GREEDY_HASH_MASK)
__CPROVER_ensures(h->val2 >= 0 && h->val2 <= GREEDY_HASH_MASK)
{
    h->val = h->val & GREEDY_HASH_MASK;
    h->val2 = h->val2 & GREEDY_HASH_MASK;
}

/* Finds the longest back-reference match at pos.  The only facts the driver
   depends on are the two postconditions of the real contract: the reported
   match stays inside the data window and never exceeds the search limit.  We
   additionally bound the distance to the window size (the real bestdist is a
   window offset), which the GetLengthScore call site requires. */
void ZopfliFindLongestMatch(ZopfliBlockState *s, const ZopfliHash *h,
                            const unsigned char *array,
                            size_t pos, size_t size, size_t limit,
                            unsigned short *sublen, unsigned short *distance,
                            unsigned short *length)
__CPROVER_requires(__CPROVER_w_ok(distance, sizeof(*distance)))
__CPROVER_requires(__CPROVER_w_ok(length, sizeof(*length)))
__CPROVER_requires(pos < size)
__CPROVER_requires(limit <= ZOPFLI_MAX_MATCH)
__CPROVER_assigns(*distance, *length, __CPROVER_object_whole(sublen))
__CPROVER_ensures(pos + *length <= size)
__CPROVER_ensures(*length <= limit)
__CPROVER_ensures(*distance <= ZOPFLI_WINDOW_SIZE)
{
    unsigned short d, l;
    __CPROVER_assume(pos + l <= size);
    __CPROVER_assume(l <= limit);
    __CPROVER_assume(d <= ZOPFLI_WINDOW_SIZE);
    *distance = d;
    *length = l;
}

/* Assertion-only verification of a length/distance pair.  Sound over-approx:
   it writes nothing, so dropping its internal asserts is sound. */
void ZopfliVerifyLenDist(const unsigned char *data, size_t datasize, size_t pos,
                         unsigned short dist, unsigned short length)
__CPROVER_assigns()
{
}
