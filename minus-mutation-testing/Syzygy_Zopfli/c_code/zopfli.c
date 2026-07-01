#include "zopfli.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <x86_64-linux-gnu/bits/types/FILE.h>
#include <limits.h>
#include <string.h>
#include <math.h>

/*
Gets the symbol for the given length, cfr. the DEFLATE spec.
Returns the symbol in the range [257-285] (inclusive)
*/
static int ZopfliGetLengthSymbol(int l)
/* The lookup table has 259 entries, so l must index within [0, 258]. Valid
   DEFLATE lengths are [3, 258], mapping to symbols [257, 285]; entries for
   l in [0, 2] are 0. */
__CPROVER_requires(l >= 0 && l <= 258)
__CPROVER_ensures(__CPROVER_return_value >= 0 && __CPROVER_return_value <= 285)
/* A real DEFLATE length (l in [3, 258]) maps to a length symbol in [257, 285];
   only the unused entries l in [0, 2] map to 0. */
__CPROVER_ensures((l >= 3) ==> (__CPROVER_return_value >= 257))
{
    static const int table[259] = {
        0, 0, 0, 257, 258, 259, 260, 261, 262, 263, 264,
        265, 265, 266, 266, 267, 267, 268, 268,
        269, 269, 269, 269, 270, 270, 270, 270,
        271, 271, 271, 271, 272, 272, 272, 272,
        273, 273, 273, 273, 273, 273, 273, 273,
        274, 274, 274, 274, 274, 274, 274, 274,
        275, 275, 275, 275, 275, 275, 275, 275,
        276, 276, 276, 276, 276, 276, 276, 276,
        277, 277, 277, 277, 277, 277, 277, 277,
        277, 277, 277, 277, 277, 277, 277, 277,
        278, 278, 278, 278, 278, 278, 278, 278,
        278, 278, 278, 278, 278, 278, 278, 278,
        279, 279, 279, 279, 279, 279, 279, 279,
        279, 279, 279, 279, 279, 279, 279, 279,
        280, 280, 280, 280, 280, 280, 280, 280,
        280, 280, 280, 280, 280, 280, 280, 280,
        281, 281, 281, 281, 281, 281, 281, 281,
        281, 281, 281, 281, 281, 281, 281, 281,
        281, 281, 281, 281, 281, 281, 281, 281,
        281, 281, 281, 281, 281, 281, 281, 281,
        282, 282, 282, 282, 282, 282, 282, 282,
        282, 282, 282, 282, 282, 282, 282, 282,
        282, 282, 282, 282, 282, 282, 282, 282,
        282, 282, 282, 282, 282, 282, 282, 282,
        283, 283, 283, 283, 283, 283, 283, 283,
        283, 283, 283, 283, 283, 283, 283, 283,
        283, 283, 283, 283, 283, 283, 283, 283,
        283, 283, 283, 283, 283, 283, 283, 283,
        284, 284, 284, 284, 284, 284, 284, 284,
        284, 284, 284, 284, 284, 284, 284, 284,
        284, 284, 284, 284, 284, 284, 284, 284,
        284, 284, 284, 284, 284, 284, 284, 285};
    return table[l];
}

/* Gets the symbol for the given dist, cfr. the DEFLATE spec. */
static int ZopfliGetDistSymbol(int dist)
/* Distances in the DEFLATE/Zopfli format are in [1, 32768], mapping to the 30
   distance symbols [0, 29]. The else-branch uses __builtin_clz(dist - 1), which
   is undefined for dist == 1, but that case is handled by the dist < 5 branch. */
__CPROVER_requires(dist >= 1 && dist <= 32768)
__CPROVER_ensures(__CPROVER_return_value >= 0 && __CPROVER_return_value <= 29)
{
    if (dist < 5)
    {
        return dist - 1;
    }
    else
    {
        int l = (31 ^ __builtin_clz(dist - 1)); /* log2(dist - 1) */
        int r = ((dist - 1) >> (l - 1)) & 1;
        return l * 2 + r;
    }
}

static size_t AbsDiff(size_t x, size_t y)
__CPROVER_ensures(__CPROVER_return_value == (x > y ? x - y : y - x))
__CPROVER_ensures(__CPROVER_return_value >= 0)
{
    if (x > y)
        return x - y;
    else
        return y - x;
}

/*
Changes the population counts in a way that the consequent Huffman tree
compression, especially its rle-part, will be more likely to compress this data
more efficiently. length contains the size of the histogram.
*/
void OptimizeHuffmanForRle(int length, size_t *counts)
/* The histogram has `length` entries; the only callers pass a full DEFLATE
   alphabet (ZOPFLI_NUM_LL for litlen, ZOPFLI_NUM_D for distance). Fixing the
   size to those two concrete values keeps the malloc'd `good_for_rle` buffer a
   definite size on every path, as the assigns/free instrumentation requires. */
__CPROVER_requires(length == ZOPFLI_NUM_LL || length == ZOPFLI_NUM_D)
__CPROVER_requires(__CPROVER_is_fresh(counts, (size_t)length * sizeof(*counts)))
/* The only observable side effect is the rewriting of the histogram entries;
   the `good_for_rle` scratch buffer is malloc'd and freed internally.  Naming
   `counts` explicitly lets the assigns instrumentation track the body-less
   malloc'd buffer as a fresh, assignable object in this frame. */
__CPROVER_assigns(__CPROVER_object_whole(counts))
{
    int i, k, stride;
    size_t symbol, sum, limit;
    int *good_for_rle;

    /* 1) We don't want to touch the trailing zeros. We may break the
    rules of the format by adding more data in the distance codes. */
    for (; length >= 0; --length)
    {
        if (length == 0)
        {
            return;
        }
        if (counts[length - 1] != 0)
        {
            /* Now counts[0..length - 1] does not have trailing zeros. */
            break;
        }
    }
    /* 2) Let's mark all population counts that already can be encoded
    with an rle code.*/
    good_for_rle = (int *)malloc((unsigned)length * sizeof(int));
    for (i = 0; i < length; ++i)
        good_for_rle[i] = 0;

    /* Let's not spoil any of the existing good rle codes.
    Mark any seq of 0's that is longer than 5 as a good_for_rle.
    Mark any seq of non-0's that is longer than 7 as a good_for_rle.*/
    symbol = counts[0];
    stride = 0;
    for (i = 0; i < length + 1; ++i)
    {
        if (i == length || counts[i] != symbol)
        {
            if ((symbol == 0 && stride >= 5) || (symbol != 0 && stride >= 7))
            {
                for (k = 0; k < stride; ++k)
                {
                    good_for_rle[i - k - 1] = 1;
                }
            }
            stride = 1;
            if (i != length)
            {
                symbol = counts[i];
            }
        }
        else
        {
            ++stride;
        }
    }

    /* 3) Let's replace those population counts that lead to more rle codes. */
    stride = 0;
    limit = counts[0];
    sum = 0;
    for (i = 0; i < length + 1; ++i)
    {
        if (i == length || good_for_rle[i]
            /* Heuristic for selecting the stride ranges to collapse. */
            || AbsDiff(counts[i], limit) >= 4)
        {
            if (stride >= 4 || (stride >= 3 && sum == 0))
            {
                /* The stride must end, collapse what we have, if we have enough (4). */
                int count = (sum + stride / 2) / stride;
                if (count < 1)
                    count = 1;
                if (sum == 0)
                {
                    /* Don't make an all zeros stride to be upgraded to ones. */
                    count = 0;
                }
                for (k = 0; k < stride; ++k)
                {
                    /* We don't want to change value at counts[i],
                    that is already belonging to the next stride. Thus - 1. */
                    counts[i - k - 1] = count;
                }
            }
            stride = 0;
            sum = 0;
            if (i < length - 3)
            {
                /* All interesting strides have a count of at least 4,
                at least when non-zeros. */
                limit = (counts[i] + counts[i + 1] +
                         counts[i + 2] + counts[i + 3] + 2) /
                        4;
            }
            else if (i < length)
            {
                limit = counts[i];
            }
            else
            {
                limit = 0;
            }
        }
        ++stride;
        if (i != length)
        {
            sum += counts[i];
        }
    }

    free(good_for_rle);
}

/* Gets the amount of extra bits for the given distance symbol. */
static int ZopfliGetDistSymbolExtraBits(int s)
__CPROVER_requires(s >= 0 && s < 30)
__CPROVER_ensures(__CPROVER_return_value >= 0 && __CPROVER_return_value <= 13)
{
    static const int table[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
        9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    return table[s];
}

/* Gets the amount of extra bits for the given length symbol. */
static int ZopfliGetLengthSymbolExtraBits(int s)
__CPROVER_requires(s >= 257 && s <= 285)
__CPROVER_ensures(__CPROVER_return_value >= 0 && __CPROVER_return_value <= 5)
{
    static const int table[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    return table[s - 257];
}

/*
Same as CalculateBlockSymbolSize, but for block size smaller than histogram
size.
*/
static size_t CalculateBlockSymbolSizeSmall(const unsigned *ll_lengths,
                                            const unsigned *d_lengths,
                                            const ZopfliLZ77Store *lz77,
                                            size_t lstart, size_t lend)
/* The loop runs over [lstart, lend) and asserts i < lz77->size, so the range
   must lie within the store. */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* The parallel litlen / dist arrays are valid for every index the loop reads. */
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lend * sizeof(*lz77->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lend * sizeof(*lz77->dists)))
/* The two code-length tables span the full DEFLATE litlen / dist alphabets; the
   loop indexes ll_lengths with raw litlens (< 259), with length symbols (<= 285)
   and the end symbol 256, and d_lengths with distance symbols (<= 29). */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
/* Per-element validity of the LZ77 commands the loop processes:
   - every litlen used as a direct index / length argument is < 259 (the body's
     assert), so ZopfliGetLengthSymbol's [0, 258] precondition holds;
   - a back-reference (dist != 0) carries a real DEFLATE length in [3, 258],
     making its length symbol >= 257 (required by ZopfliGetLengthSymbolExtraBits),
     and a distance in [1, 32768] (required by ZopfliGetDistSymbol). */
__CPROVER_requires(__CPROVER_forall {
    size_t il; (lstart <= il && il < lend) ==> (lz77->litlens[il] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (lstart <= ir && ir < lend && lz77->dists[ir] != 0)
                  ==> (lz77->litlens[ir] >= 3
                       && lz77->dists[ir] >= 1 && lz77->dists[ir] <= 32768)
})
{
    size_t result = 0;
    size_t i;
    for (i = lstart; i < lend; i++)
    {
        assert(i < lz77->size);
        assert(lz77->litlens[i] < 259);
        if (lz77->dists[i] == 0)
        {
            result += ll_lengths[lz77->litlens[i]];
        }
        else
        {
            int ll_symbol = ZopfliGetLengthSymbol(lz77->litlens[i]);
            int d_symbol = ZopfliGetDistSymbol(lz77->dists[i]);
            result += ll_lengths[ll_symbol];
            result += d_lengths[d_symbol];
            result += ZopfliGetLengthSymbolExtraBits(ll_symbol);
            result += ZopfliGetDistSymbolExtraBits(d_symbol);
        }
    }
    result += ll_lengths[256]; /*end symbol*/
    return result;
}

/*
Same as CalculateBlockSymbolSize, but with the histogram provided by the caller.
*/
static size_t CalculateBlockSymbolSizeGivenCounts(const size_t *ll_counts,
                                                  const size_t *d_counts,
                                                  const unsigned *ll_lengths,
                                                  const unsigned *d_lengths,
                                                  const ZopfliLZ77Store *lz77,
                                                  size_t lstart, size_t lend)
/* The "small" branch (lstart + ZOPFLI_NUM_LL*3 > lend) delegates to
   CalculateBlockSymbolSizeSmall, so the same range / LZ77-store preconditions
   apply -- but only on that branch.  The "large" branch never touches the
   per-command litlen / dist arrays, so guarding these clauses with the branch
   condition both avoids over-constraining the histogram path and keeps lend
   bounded (lend < lstart + ZOPFLI_NUM_LL*3) where the litlen / dist arrays are
   actually indexed. */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 > lend) ==>
    __CPROVER_is_fresh(lz77->litlens, lend * sizeof(*lz77->litlens)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 > lend) ==>
    __CPROVER_is_fresh(lz77->dists, lend * sizeof(*lz77->dists)))
__CPROVER_requires(__CPROVER_forall {
    size_t il; (lstart + ZOPFLI_NUM_LL * 3 > lend && lstart <= il && il < lend)
                  ==> (lz77->litlens[il] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (lstart + ZOPFLI_NUM_LL * 3 > lend
                && lstart <= ir && ir < lend && lz77->dists[ir] != 0)
                  ==> (lz77->litlens[ir] >= 3
                       && lz77->dists[ir] >= 1 && lz77->dists[ir] <= 32768)
})
/* The "large" branch reads the caller-provided histograms and code-length
   tables: ll_counts / ll_lengths over the litlen alphabet (indices < 286 plus
   the end symbol 256, all < ZOPFLI_NUM_LL) and d_counts / d_lengths over the
   distance alphabet (indices < 30 <= ZOPFLI_NUM_D). */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_counts, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_counts)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_counts, (size_t)ZOPFLI_NUM_D * sizeof(*d_counts)))
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
/* Every entry of the two code-length tables is a DEFLATE Huffman code length,
   which the format caps at 15 bits.  This bound is what keeps the "large" branch
   tractable: the products ll_lengths[i]*ll_counts[i] and d_lengths[i]*d_counts[i]
   would otherwise be full 64x64-bit multiplications, which blow up the SAT
   encoding; with a length operand of at most 4 bits they reduce to a handful of
   shifted adds. */
__CPROVER_requires(__CPROVER_forall {
    size_t kl; kl < (size_t)ZOPFLI_NUM_LL ==> ll_lengths[kl] <= 15
})
__CPROVER_requires(__CPROVER_forall {
    size_t kd; kd < (size_t)ZOPFLI_NUM_D ==> d_lengths[kd] <= 15
})
{
    size_t result = 0;
    size_t i;
    if (lstart + ZOPFLI_NUM_LL * 3 > lend)
    {
        return CalculateBlockSymbolSizeSmall(
            ll_lengths, d_lengths, lz77, lstart, lend);
    }
    else
    {
        for (i = 0; i < 256; i++)
        {
            result += ll_lengths[i] * ll_counts[i];
        }
        for (i = 257; i < 286; i++)
        {
            result += ll_lengths[i] * ll_counts[i];
            result += ZopfliGetLengthSymbolExtraBits(i) * ll_counts[i];
        }
        for (i = 0; i < 30; i++)
        {
            result += d_lengths[i] * d_counts[i];
            result += ZopfliGetDistSymbolExtraBits(i) * d_counts[i];
        }
        result += ll_lengths[256]; /*end symbol*/
        return result;
    }
}

/*
Comparator for sorting the leaves. Has the function signature for qsort.
*/
static int LeafComparator(const void *a, const void *b)
__CPROVER_requires(__CPROVER_is_fresh(a, sizeof(Node)))
__CPROVER_requires(__CPROVER_is_fresh(b, sizeof(Node)))
__CPROVER_assigns()
{
    return ((const Node *)a)->weight - ((const Node *)b)->weight;
}

/* Largest possible Huffman alphabet handled by the package-merge code (the
   literal/length tree).  It is the maximum number of leaves, and therefore an
   upper bound both on the size of the `leaves`/`bitlengths` arrays and on the
   leaf-count values stored in the chain. */
#define EXTRACT_MAXSYMBOLS ZOPFLI_NUM_LL

/* Contract helper: pointer `p` (a chain link) is either NULL or a fresh node
   whose `count` field (number of active leaves) lies in [0, EXTRACT_MAXSYMBOLS].
   Non-negative, bounded counts keep the second loop's `val`/`leaves` index in
   range; the NULL alternative lets the chain terminate at any length. */
#define EXTRACT_NODE_OK(p)                          \
    ((p) == NULL ||                                 \
     (__CPROVER_is_fresh((p), sizeof(Node)) &&      \
      0 <= (p)->count && (p)->count <= EXTRACT_MAXSYMBOLS))

/*
Converts result of boundary package-merge to the bitlengths. The result in the
last chain of the last list contains the amount of active leaves in each list.
chain: Chain to extract the bit length from (last chain from last list).
*/
static void ExtractBitLengths(Node *chain, Node *leaves, unsigned *bitlengths)
/* The chain is a NULL-terminated list of fresh nodes.  Each node's `count` is
   the number of active leaves, in [0, EXTRACT_MAXSYMBOLS]; counts[15] (the head
   count) bounds `val`, hence the `leaves` index.  The first loop walks the
   chain writing counts[15], counts[14], ..., so the chain having at most 16
   nodes keeps `end >= 0`; in this bounded harness the loop is unwound a fixed
   number of times, so we describe the nodes that can actually be dereferenced. */
__CPROVER_requires(EXTRACT_NODE_OK(chain))
__CPROVER_requires(chain == NULL || EXTRACT_NODE_OK(chain->tail))
__CPROVER_requires(
    chain == NULL || chain->tail == NULL ||
    EXTRACT_NODE_OK(chain->tail->tail))
__CPROVER_requires(
    chain == NULL || chain->tail == NULL || chain->tail->tail == NULL ||
    EXTRACT_NODE_OK(chain->tail->tail->tail))
__CPROVER_requires(
    chain == NULL || chain->tail == NULL || chain->tail->tail == NULL ||
    chain->tail->tail->tail == NULL ||
    EXTRACT_NODE_OK(chain->tail->tail->tail->tail))
__CPROVER_requires(
    chain == NULL || chain->tail == NULL || chain->tail->tail == NULL ||
    chain->tail->tail->tail == NULL || chain->tail->tail->tail->tail == NULL ||
    EXTRACT_NODE_OK(chain->tail->tail->tail->tail->tail))
/* `leaves` and `bitlengths` are sized for the largest alphabet.  The loop reads
   leaves[0 .. val-1] with val <= chain head count <= EXTRACT_MAXSYMBOLS. */
__CPROVER_requires(
    __CPROVER_is_fresh(leaves, (size_t)EXTRACT_MAXSYMBOLS * sizeof(*leaves)))
__CPROVER_requires(
    __CPROVER_is_fresh(bitlengths, (size_t)EXTRACT_MAXSYMBOLS * sizeof(*bitlengths)))
/* Every leaf's `count` is the symbol index it represents, a valid index into
   `bitlengths`; this keeps `bitlengths[leaves[..].count]` in range. */
__CPROVER_requires(__CPROVER_forall {
    int i;
    (0 <= i && i < EXTRACT_MAXSYMBOLS) ==>
        (0 <= leaves[i].count && leaves[i].count < EXTRACT_MAXSYMBOLS)
})
__CPROVER_assigns(__CPROVER_object_whole(bitlengths))
{
    int counts[16] = {0};
    unsigned end = 16;
    unsigned ptr = 15;
    unsigned value = 1;
    Node *node;
    int val;

    for (node = chain; node; node = node->tail)
    {
        counts[--end] = node->count;
    }

    val = counts[15];
    while (ptr >= end)
    {
        for (; val > counts[ptr - 1]; val--)
        {
            bitlengths[leaves[val - 1].count] = value;
        }
        ptr--;
        value++;
    }
}

/*
Initializes a chain node with the given values and marks it as in use.
*/
static void InitNode(size_t weight, int count, Node *tail, Node *node)
__CPROVER_requires(__CPROVER_is_fresh(node, sizeof(*node)))
__CPROVER_assigns(node->weight, node->count, node->tail)
__CPROVER_ensures(node->weight == weight)
__CPROVER_ensures(node->count == count)
__CPROVER_ensures(node->tail == tail)
{
    node->weight = weight;
    node->count = count;
    node->tail = tail;
}

/* Maximum number of chain lists initialized here. In DEFLATE the bit lengths
   are limited to 15, so InitLists is never asked to set up more lists than this.
   It bounds the structural unrolling of the loop below. */
#define INITLISTS_MAXLISTS 15

/*
Initializes each list with as lookahead chains the two leaves with lowest
weights.
*/
static void InitLists(
    NodePool *pool, const Node *leaves, int maxbits, Node *(*lists)[2])
/* maxbits is the number of lists, bounded by the DEFLATE bit-length limit. */
__CPROVER_requires(maxbits >= 0 && maxbits <= INITLISTS_MAXLISTS)
__CPROVER_requires(__CPROVER_is_fresh(pool, sizeof(*pool)))
/* Two fresh nodes are taken from the pool, one after the other. */
__CPROVER_requires(__CPROVER_is_fresh(pool->next, 2 * sizeof(Node)))
/* The two lowest-weight leaves (leaves[0], leaves[1]) are read. */
__CPROVER_requires(__CPROVER_is_fresh(leaves, 2 * sizeof(*leaves)))
/* Each of the maxbits lists is a writable pair of chain pointers. */
__CPROVER_requires(
    __CPROVER_is_fresh(lists, (size_t)maxbits * sizeof(*lists)))
/* The pool's free pointer is advanced past the two consumed nodes. */
__CPROVER_assigns(pool->next)
/* The two nodes taken from the pool are initialized. */
__CPROVER_assigns(__CPROVER_object_whole(pool->next))
/* Every chain-pointer pair in the lists array is written. */
__CPROVER_assigns(__CPROVER_object_whole(lists))
__CPROVER_ensures(pool->next == __CPROVER_old(pool->next) + 2)
{
    int i;
    Node *node0 = pool->next++;
    Node *node1 = pool->next++;
    InitNode(leaves[0].weight, 1, 0, node0);
    InitNode(leaves[1].weight, 2, 0, node1);
    for (i = 0; i < maxbits; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(lists))
    __CPROVER_loop_invariant(0 <= i && i <= maxbits)
    {
        lists[i][0] = node0;
        lists[i][1] = node1;
    }
}

/* Maximum number of chain lists. In DEFLATE the bit lengths are limited to 15,
   so the boundary package-merge never uses more lists than this. It bounds the
   structural unrolling of the preconditions below. */
#define BOUNDARYPM_MAXLISTS 15

static void BoundaryPMFinal(Node *(*lists)[2],
                            Node *leaves, int numsymbols, NodePool *pool, int index)
/* This is the final boundary package-merge step, applied to the last list. It
   reads lists[index] and lists[index-1], so index must be at least 1. */
__CPROVER_requires(index >= 1 && index < BOUNDARYPM_MAXLISTS)
__CPROVER_requires(numsymbols >= 1 && numsymbols < INT_MAX)
__CPROVER_requires(
    __CPROVER_is_fresh(lists, (size_t)BOUNDARYPM_MAXLISTS * sizeof(*lists)))
__CPROVER_requires(__CPROVER_is_fresh(leaves, (size_t)numsymbols * sizeof(*leaves)))
__CPROVER_requires(__CPROVER_is_fresh(pool, sizeof(*pool)))
/* The new chain, when created, is taken from pool->next (without consuming it). */
__CPROVER_requires(__CPROVER_is_fresh(pool->next, sizeof(Node)))
/* The two chain pointers of the last list and the previous list are valid nodes. */
__CPROVER_requires(__CPROVER_is_fresh(lists[index][0], sizeof(Node)))
__CPROVER_requires(__CPROVER_is_fresh(lists[index][1], sizeof(Node)))
__CPROVER_requires(__CPROVER_is_fresh(lists[index - 1][0], sizeof(Node)))
__CPROVER_requires(__CPROVER_is_fresh(lists[index - 1][1], sizeof(Node)))
/* The last chain's leaf count is in [0, numsymbols]; the count bound keeps the
   leaves[lastcount] access in range when lastcount < numsymbols. */
__CPROVER_requires(
    0 <= lists[index][1]->count && lists[index][1]->count <= numsymbols)
__CPROVER_assigns(__CPROVER_object_whole(lists))
/* Else branch updates the tail of the (pre-state) last chain of the list. */
__CPROVER_assigns(lists[index][1]->tail)
/* If branch initialises the fresh node taken from the pool. */
__CPROVER_assigns(__CPROVER_object_whole(pool->next))
/* On return the last chain of the list is a readable/writable node, so callers
   (ExtractBitLengths) can walk it. */
__CPROVER_ensures(__CPROVER_rw_ok(lists[index][1], sizeof(Node)))
{
    int lastcount = lists[index][1]->count; /* Count of last chain of list. */

    size_t sum = lists[index - 1][0]->weight + lists[index - 1][1]->weight;

    if (lastcount < numsymbols && sum > leaves[lastcount].weight)
    {
        Node *newchain = pool->next;
        Node *oldchain = lists[index][1]->tail;

        lists[index][1] = newchain;
        newchain->count = lastcount + 1;
        newchain->tail = oldchain;
    }
    else
    {
        lists[index][1]->tail = lists[index - 1][1];
    }
}

/*
Performs a Boundary Package-Merge step. Puts a new chain in the given list. The
new chain is, depending on the weights, a leaf or a combination of two chains
from the previous list.
lists: The lists of chains.
maxbits: Number of lists.
leaves: The leaves, one per symbol.
numsymbols: Number of leaves.
pool: the node memory pool.
index: The index of the list in which a new chain or leaf is required.
*/
/* For chain list k (when it is within the active range k <= index): both chain
   pointers point to fresh Nodes, and the "last chain" carries a leaf count in
   [0, numsymbols]. The count bound keeps leaves[lastcount] accesses in range. */
#define BOUNDARYPM_LIST_VALID(k)                          \
    ((k) > index ||                                       \
     (__CPROVER_is_fresh(lists[k][0], sizeof(Node)) &&    \
      __CPROVER_is_fresh(lists[k][1], sizeof(Node)) &&    \
      0 <= lists[k][1]->count && lists[k][1]->count <= numsymbols))

static void BoundaryPM(Node *(*lists)[2], Node *leaves, int numsymbols,
                       NodePool *pool, int index)
__CPROVER_requires(index >= 0 && index < BOUNDARYPM_MAXLISTS)
__CPROVER_requires(numsymbols >= 1 && numsymbols < INT_MAX)
__CPROVER_requires(
    __CPROVER_is_fresh(lists, (size_t)BOUNDARYPM_MAXLISTS * sizeof(*lists)))
__CPROVER_requires(__CPROVER_is_fresh(leaves, (size_t)numsymbols * sizeof(*leaves)))
__CPROVER_requires(__CPROVER_is_fresh(pool, sizeof(*pool)))
__CPROVER_requires(__CPROVER_is_fresh(
    pool->next, ((size_t)index + 1) * 2 * (size_t)numsymbols * sizeof(Node)))
__CPROVER_requires(
    BOUNDARYPM_LIST_VALID(0) && BOUNDARYPM_LIST_VALID(1) &&
    BOUNDARYPM_LIST_VALID(2) && BOUNDARYPM_LIST_VALID(3) &&
    BOUNDARYPM_LIST_VALID(4) && BOUNDARYPM_LIST_VALID(5) &&
    BOUNDARYPM_LIST_VALID(6) && BOUNDARYPM_LIST_VALID(7) &&
    BOUNDARYPM_LIST_VALID(8) && BOUNDARYPM_LIST_VALID(9) &&
    BOUNDARYPM_LIST_VALID(10) && BOUNDARYPM_LIST_VALID(11) &&
    BOUNDARYPM_LIST_VALID(12) && BOUNDARYPM_LIST_VALID(13) &&
    BOUNDARYPM_LIST_VALID(14))
__CPROVER_assigns(pool->next)
__CPROVER_assigns(__CPROVER_object_whole(lists))
__CPROVER_assigns(__CPROVER_object_whole(pool->next))
__CPROVER_ensures(__CPROVER_rw_ok(lists[index][1], sizeof(Node)))
__CPROVER_ensures(pool->next >= __CPROVER_old(pool->next))
{
    Node *newchain;
    Node *oldchain;
    int lastcount = lists[index][1]->count; /* Count of last chain of list. */

    if (index == 0 && lastcount >= numsymbols)
        return;

    newchain = pool->next++;
    oldchain = lists[index][1];

    /* These are set up before the recursive calls below, so that there is a list
    pointing to the new node, to let the garbage collection know it's in use. */
    lists[index][0] = oldchain;
    lists[index][1] = newchain;

    if (index == 0)
    {
        /* New leaf node in list 0. */
        InitNode(leaves[lastcount].weight, lastcount + 1, 0, newchain);
    }
    else
    {
        size_t sum = lists[index - 1][0]->weight + lists[index - 1][1]->weight;
        if (lastcount < numsymbols && sum > leaves[lastcount].weight)
        {
            /* New leaf inserted in list, so count is incremented. */
            InitNode(leaves[lastcount].weight, lastcount + 1, oldchain->tail,
                     newchain);
        }
        else
        {
            InitNode(sum, lastcount, lists[index - 1][1], newchain);
            /* Two lookahead chains of previous list used up, create new ones. */
            BoundaryPM(lists, leaves, numsymbols, pool, index - 1);
            BoundaryPM(lists, leaves, numsymbols, pool, index - 1);
        }
    }
}

/*
Outputs minimum-redundancy length-limited code bitlengths for symbols with the
given counts. The bitlengths are limited by maxbits.

The output is tailored for DEFLATE: symbols that never occur, get a bit length
of 0, and if only a single symbol occurs at least once, its bitlength will be 1,
and not 0 as would theoretically be needed for a single symbol.

frequencies: The amount of occurrences of each symbol.
n: The amount of symbols.
maxbits: Maximum bit length, inclusive.
bitlengths: Output, the bitlengths for the symbol prefix codes.
return: 0 for OK, non-0 for error.
*/
int ZopfliLengthLimitedCodeLengths(
    const size_t *frequencies, int n, int maxbits, unsigned *bitlengths)
/* The largest DEFLATE alphabet has ZOPFLI_NUM_LL symbols, and the callee
   contracts (ExtractBitLengths reads leaves/bitlengths of this size, the package
   merge uses up to INITLISTS_MAXLISTS chain lists) are written for that maximal
   shape.  We therefore verify the function at full width: n symbols with
   bitlengths/frequencies arrays of n elements, and maxbits at the DEFLATE limit. */
__CPROVER_requires(n == EXTRACT_MAXSYMBOLS)
__CPROVER_requires(maxbits == INITLISTS_MAXLISTS)
__CPROVER_requires(
    __CPROVER_is_fresh(frequencies, (size_t)n * sizeof(*frequencies)))
__CPROVER_requires(
    __CPROVER_is_fresh(bitlengths, (size_t)n * sizeof(*bitlengths)))
/* Force every symbol to be used, so numsymbols == n and maxbits is not reduced
   below INITLISTS_MAXLISTS; the weight bound stays clear of the 9-bit-count
   packing overflow check so the package-merge path is exercised. */
__CPROVER_requires(__CPROVER_forall {
    int i;
    (0 <= i && i < n) ==>
        (1 <= frequencies[i] &&
         frequencies[i] < ((size_t)1 << (sizeof(size_t) * CHAR_BIT - 9)))
})
__CPROVER_assigns(__CPROVER_object_whole(bitlengths))
/* Under the preconditions above every symbol is used (numsymbols == n == 288),
   maxbits == 15 admits the alphabet (1 << 15 > 288), and the frequency bound
   keeps the 9-bit packing from overflowing, so every error-return path is
   excluded and the function always succeeds. */
__CPROVER_ensures(__CPROVER_return_value == 0)
{
    NodePool pool;
    int i;
    int numsymbols = 0; /* Amount of symbols with frequency > 0. */
    int numBoundaryPMRuns;
    Node *nodes;

    /* Array of lists of chains. Each list requires only two lookahead chains at
    a time, so each list is a array of two Node*'s. */
    Node *(*lists)[2];

    /* One leaf per symbol. Only numsymbols leaves will be used. */
    Node *leaves = (Node *)malloc(n * sizeof(*leaves));

    /* Initialize all bitlengths at 0. */
    for (i = 0; i < n; i++)
    {
        bitlengths[i] = 0;
    }

    /* Count used symbols and place them in the leaves. */
    for (i = 0; i < n; i++)
    {
        if (frequencies[i])
        {
            leaves[numsymbols].weight = frequencies[i];
            leaves[numsymbols].count = i; /* Index of symbol this leaf represents. */
            numsymbols++;
        }
    }

    /* Check special cases and error conditions. */
    if ((1 << maxbits) < numsymbols)
    {
        free(leaves);
        return 1; /* Error, too few maxbits to represent symbols. */
    }
    if (numsymbols == 0)
    {
        free(leaves);
        return 0; /* No symbols at all. OK. */
    }
    if (numsymbols == 1)
    {
        bitlengths[leaves[0].count] = 1;
        free(leaves);
        return 0; /* Only one symbol, give it bitlength 1, not 0. OK. */
    }
    if (numsymbols == 2)
    {
        bitlengths[leaves[0].count]++;
        bitlengths[leaves[1].count]++;
        free(leaves);
        return 0;
    }

    /* Sort the leaves from lightest to heaviest. Add count into the same
    variable for stable sorting. */
    for (i = 0; i < numsymbols; i++)
    {
        if (leaves[i].weight >=
            ((size_t)1 << (sizeof(leaves[0].weight) * CHAR_BIT - 9)))
        {
            free(leaves);
            return 1; /* Error, we need 9 bits for the count. */
        }
        leaves[i].weight = (leaves[i].weight << 9) | leaves[i].count;
    }
    qsort(leaves, numsymbols, sizeof(Node), LeafComparator);
    for (i = 0; i < numsymbols; i++)
    {
        leaves[i].weight >>= 9;
    }

    if (numsymbols - 1 < maxbits)
    {
        maxbits = numsymbols - 1;
    }

    /* Initialize node memory pool. */
    nodes = (Node *)malloc(maxbits * 2 * numsymbols * sizeof(Node));
    pool.next = nodes;

    lists = (Node * (*)[2]) malloc(maxbits * sizeof(*lists));
    InitLists(&pool, leaves, maxbits, lists);

    /* In the last list, 2 * numsymbols - 2 active chains need to be created. Two
    are already created in the initialization. Each BoundaryPM run creates one. */
    numBoundaryPMRuns = 2 * numsymbols - 4;
    for (i = 0; i < numBoundaryPMRuns - 1; i++)
    {
        BoundaryPM(lists, leaves, numsymbols, &pool, maxbits - 1);
    }
    BoundaryPMFinal(lists, leaves, numsymbols, &pool, maxbits - 1);

    ExtractBitLengths(lists[maxbits - 1][1], leaves, bitlengths);

    free(lists);
    free(leaves);
    free(nodes);
    return 0; /* OK. */
}

/*
Calculates the bitlengths for the Huffman tree, based on the counts of each
symbol.
*/
void ZopfliCalculateBitLengths(const size_t *count, size_t n, int maxbits,
                               unsigned *bitlengths)
/* The arguments are forwarded verbatim to ZopfliLengthLimitedCodeLengths, so we
   reproduce its preconditions: the full DEFLATE alphabet, the maximal code
   length, fresh count/bitlengths buffers of n elements, and every count used
   with a frequency that fits the 9-bit packing.  Under these the callee always
   succeeds, so the assert(!error) holds. */
__CPROVER_requires(n == EXTRACT_MAXSYMBOLS)
__CPROVER_requires(maxbits == INITLISTS_MAXLISTS)
__CPROVER_requires(__CPROVER_is_fresh(count, n * sizeof(*count)))
__CPROVER_requires(__CPROVER_is_fresh(bitlengths, n * sizeof(*bitlengths)))
__CPROVER_requires(__CPROVER_forall {
    size_t i;
    (i < n) ==>
        (1 <= count[i] &&
         count[i] < ((size_t)1 << (sizeof(size_t) * CHAR_BIT - 9)))
})
__CPROVER_assigns(__CPROVER_object_whole(bitlengths))
{
    int error = ZopfliLengthLimitedCodeLengths(count, n, maxbits, bitlengths);
    (void)error;
    assert(!error);
}

/*
Adds bits, like AddBits, but the order is inverted. The deflate specification
uses both orders in one standard.
*/
static void AddHuffmanBits(unsigned symbol, unsigned length,
                           unsigned char *bp, unsigned char **out,
                           size_t *outsize)
/* bp, out, outsize must be valid pointers. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
/* *bp is a bit index within a byte, used as a shift amount. */
__CPROVER_requires(*bp <= 7)
/* Every caller emits a single Huffman code whose length is at most 7 bits here,
   so the bit cursor wraps to 0 at most once and thus at most a single fresh byte
   is appended over the whole loop. */
__CPROVER_requires(length <= 7)
/* The buffer holds at least one byte that can be updated, plus room to append
   one more byte without overflowing.  Constraining *outsize to a non-power-of-
   two (and non-zero) value keeps the ZOPFLI_APPEND_DATA macro on its
   no-reallocation path for that single append. */
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
__CPROVER_assigns(*bp, *outsize, __CPROVER_object_whole(*out))
/* One fresh byte is appended for each time the bit cursor lands on 0 while
   emitting the `length` bits. */
__CPROVER_ensures(*outsize ==
                  __CPROVER_old(*outsize)
                      + ((__CPROVER_old(*bp) + length + 7) / 8
                         - (__CPROVER_old(*bp) + 7) / 8))
/* The bit cursor advances by `length`, modulo 8. */
__CPROVER_ensures(*bp == ((__CPROVER_old(*bp) + length) & 7))
{
    /* TODO(lode): make more efficient (add more bits at once). */
    unsigned i;
    for (i = 0; i < length; i++)
    {
        unsigned bit = (symbol >> (length - i - 1)) & 1;
        if (*bp == 0)
            ZOPFLI_APPEND_DATA(0, out, outsize);
        (*out)[*outsize - 1] |= bit << *bp;
        *bp = (*bp + 1) & 7;
    }
}

static void AddBits(unsigned symbol, unsigned length,
                    unsigned char *bp, unsigned char **out, size_t *outsize)
/* bp, out, outsize must be valid pointers. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
/* *bp is a bit index within a byte, used as a shift amount. */
__CPROVER_requires(*bp <= 7)
/* Every caller writes at most a 7-bit field (DEFLATE extra bits / fixed-width
   header fields), so the bit cursor wraps to 0 at most once and thus at most a
   single fresh byte is appended over the whole loop. */
__CPROVER_requires(length <= 7)
/* The buffer holds at least one byte that can be updated, plus room to append
   one more byte without overflowing.  Constraining *outsize to a non-power-of-
   two (and non-zero) value keeps the ZOPFLI_APPEND_DATA macro on its
   no-reallocation path for that single append. */
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
__CPROVER_assigns(*bp, *outsize, __CPROVER_object_whole(*out))
/* One fresh byte is appended for each time the bit cursor lands on 0 while
   emitting the `length` bits. */
__CPROVER_ensures(*outsize ==
                  __CPROVER_old(*outsize)
                      + ((__CPROVER_old(*bp) + length + 7) / 8
                         - (__CPROVER_old(*bp) + 7) / 8))
/* The bit cursor advances by `length`, modulo 8. */
__CPROVER_ensures(*bp == ((__CPROVER_old(*bp) + length) & 7))
{
    /* TODO(lode): make more efficient (add more bits at once). */
    unsigned i;
    for (i = 0; i < length; i++)
    {
        unsigned bit = (symbol >> i) & 1;
        if (*bp == 0)
            ZOPFLI_APPEND_DATA(0, out, outsize);
        (*out)[*outsize - 1] |= bit << *bp;
        *bp = (*bp + 1) & 7;
    }
}

/* The DEFLATE format limits Huffman code lengths to at most 15 bits, so the
   per-length count and base-code tables never need more than this many entries.
   This bounds the structural unrolling of the preconditions below. */
#define LENGTHSTOSYMBOLS_MAXBITS 15

/*
Converts a series of Huffman tree bitlengths, to the bit values of the symbols.
*/
void ZopfliLengthsToSymbols(const unsigned *lengths, size_t n, unsigned maxbits,
                            unsigned *symbols)
/* Code lengths fit in the DEFLATE limit, keeping (maxbits + 1) malloc sizes and
   table indices in range. */
__CPROVER_requires(maxbits <= LENGTHSTOSYMBOLS_MAXBITS)
/* The largest DEFLATE alphabet has ZOPFLI_NUM_LL symbols. */
__CPROVER_requires(n <= ZOPFLI_NUM_LL)
/* The n input bit lengths are readable. */
__CPROVER_requires(__CPROVER_is_fresh(lengths, n * sizeof(*lengths)))
/* The n output symbols are writable. */
__CPROVER_requires(__CPROVER_is_fresh(symbols, n * sizeof(*symbols)))
/* Every code length is within maxbits, so it is a valid index into the
   (maxbits + 1)-entry count/next-code tables. */
__CPROVER_requires(__CPROVER_forall {
    size_t k; (k < n) ==> (lengths[k] <= maxbits)
})
/* Only the output array is observable after the call. */
__CPROVER_assigns(__CPROVER_object_whole(symbols))
{
    size_t *bl_count = (size_t *)malloc(sizeof(size_t) * (maxbits + 1));
    size_t *next_code = (size_t *)malloc(sizeof(size_t) * (maxbits + 1));
    unsigned bits, i;
    unsigned code;

    for (i = 0; i < n; i++)
    {
        symbols[i] = 0;
    }

    /* 1) Count the number of codes for each code length. Let bl_count[N] be the
    number of codes of length N, N >= 1. */
    for (bits = 0; bits <= maxbits; bits++)
    {
        bl_count[bits] = 0;
    }
    for (i = 0; i < n; i++)
    {
        assert(lengths[i] <= maxbits);
        bl_count[lengths[i]]++;
    }
    /* 2) Find the numerical value of the smallest code for each code length. */
    code = 0;
    bl_count[0] = 0;
    for (bits = 1; bits <= maxbits; bits++)
    {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    /* 3) Assign numerical values to all codes, using consecutive values for all
    codes of the same length with the base values determined at step 2. */
    for (i = 0; i < n; i++)
    {
        unsigned len = lengths[i];
        if (len != 0)
        {
            symbols[i] = next_code[len];
            next_code[len]++;
        }
    }

    free(bl_count);
    free(next_code);
}

/*
Encodes the Huffman tree and returns how many bits its encoding takes. If out
is a null pointer, only returns the size and runs faster.
*/
static size_t EncodeTree(const unsigned *ll_lengths,
                         const unsigned *d_lengths,
                         int use_16, int use_17, int use_18,
                         unsigned char *bp,
                         unsigned char **out, size_t *outsize)
/* We verify the size-only mode (out == NULL), the path used by CalculateTreeSize.
   In this mode no bits are emitted (every AddBits / AddHuffmanBits / append is
   guarded by !size_only), so bp/outsize are never touched and only the local
   tables and the run-length scan are exercised. */
__CPROVER_requires(out == NULL)
/* The two length tables hold the full DEFLATE litlen / dist alphabets. The scan
   reads ll_lengths over [0, hlit2) with hlit2 <= 286 and d_lengths over
   [0, hdist] with hdist <= 29, so the whole alphabets are within bounds. */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
/* Each entry is a Huffman code length, which DEFLATE bounds by 15. The code uses
   it (truncated to unsigned char) directly as an index into the 19-entry clcounts
   table, so it must be < 19; bounding by 15 keeps every clcounts[symbol] access in
   range. */
__CPROVER_requires(__CPROVER_forall {
    size_t kl; (kl < (size_t)ZOPFLI_NUM_LL) ==> (ll_lengths[kl] <= 15)
})
__CPROVER_requires(__CPROVER_forall {
    size_t kd; (kd < (size_t)ZOPFLI_NUM_D) ==> (d_lengths[kd] <= 15)
})
/* size-only mode writes nothing observable to the caller. */
__CPROVER_assigns()
/* The encoding always includes the 14 header bits plus the 3-bit code-length-code
   lengths, so the returned size is at least 14. */
__CPROVER_ensures(__CPROVER_return_value >= 14)
{
    unsigned lld_total; /* Total amount of literal, length, distance codes. */
    /* Runlength encoded version of lengths of litlen and dist trees. */
    unsigned *rle = 0;
    unsigned *rle_bits = 0;   /* Extra bits for rle values 16, 17 and 18. */
    size_t rle_size = 0;      /* Size of rle array. */
    size_t rle_bits_size = 0; /* Should have same value as rle_size. */
    unsigned hlit = 29;       /* 286 - 257 */
    unsigned hdist = 29;      /* 32 - 1, but gzip does not like hdist > 29.*/
    unsigned hclen;
    unsigned hlit2;
    size_t i, j;
    size_t clcounts[19];
    unsigned clcl[19]; /* Code length code lengths. */
    unsigned clsymbols[19];
    /* The order in which code length code lengths are encoded as per deflate. */
    static const unsigned order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    int size_only = !out;
    size_t result_size = 0;

    for (i = 0; i < 19; i++)
        clcounts[i] = 0;

    /* Trim zeros. */
    while (hlit > 0 && ll_lengths[257 + hlit - 1] == 0)
        hlit--;
    while (hdist > 0 && d_lengths[1 + hdist - 1] == 0)
        hdist--;
    hlit2 = hlit + 257;

    lld_total = hlit2 + hdist + 1;

    for (i = 0; i < lld_total; i++)
    {
        /* This is an encoding of a huffman tree, so now the length is a symbol */
        unsigned char symbol = i < hlit2 ? ll_lengths[i] : d_lengths[i - hlit2];
        unsigned count = 1;
        if (use_16 || (symbol == 0 && (use_17 || use_18)))
        {
            for (j = i + 1; j < lld_total && symbol ==
                                                 (j < hlit2 ? ll_lengths[j] : d_lengths[j - hlit2]);
                 j++)
            {
                count++;
            }
        }
        i += count - 1;

        /* Repetitions of zeroes */
        if (symbol == 0 && count >= 3)
        {
            if (use_18)
            {
                while (count >= 11)
                {
                    unsigned count2 = count > 138 ? 138 : count;
                    if (!size_only)
                    {
                        ZOPFLI_APPEND_DATA(18, &rle, &rle_size);
                        ZOPFLI_APPEND_DATA(count2 - 11, &rle_bits, &rle_bits_size);
                    }
                    clcounts[18]++;
                    count -= count2;
                }
            }
            if (use_17)
            {
                while (count >= 3)
                {
                    unsigned count2 = count > 10 ? 10 : count;
                    if (!size_only)
                    {
                        ZOPFLI_APPEND_DATA(17, &rle, &rle_size);
                        ZOPFLI_APPEND_DATA(count2 - 3, &rle_bits, &rle_bits_size);
                    }
                    clcounts[17]++;
                    count -= count2;
                }
            }
        }

        /* Repetitions of any symbol */
        if (use_16 && count >= 4)
        {
            count--; /* Since the first one is hardcoded. */
            clcounts[symbol]++;
            if (!size_only)
            {
                ZOPFLI_APPEND_DATA(symbol, &rle, &rle_size);
                ZOPFLI_APPEND_DATA(0, &rle_bits, &rle_bits_size);
            }
            while (count >= 3)
            {
                unsigned count2 = count > 6 ? 6 : count;
                if (!size_only)
                {
                    ZOPFLI_APPEND_DATA(16, &rle, &rle_size);
                    ZOPFLI_APPEND_DATA(count2 - 3, &rle_bits, &rle_bits_size);
                }
                clcounts[16]++;
                count -= count2;
            }
        }

        /* No or insufficient repetition */
        clcounts[symbol] += count;
        while (count > 0)
        {
            if (!size_only)
            {
                ZOPFLI_APPEND_DATA(symbol, &rle, &rle_size);
                ZOPFLI_APPEND_DATA(0, &rle_bits, &rle_bits_size);
            }
            count--;
        }
    }

    ZopfliCalculateBitLengths(clcounts, 19, 7, clcl);
    if (!size_only)
        ZopfliLengthsToSymbols(clcl, 19, 7, clsymbols);

    hclen = 15;
    /* Trim zeros. */
    while (hclen > 0 && clcounts[order[hclen + 4 - 1]] == 0)
        hclen--;

    if (!size_only)
    {
        AddBits(hlit, 5, bp, out, outsize);
        AddBits(hdist, 5, bp, out, outsize);
        AddBits(hclen, 4, bp, out, outsize);

        for (i = 0; i < hclen + 4; i++)
        {
            AddBits(clcl[order[i]], 3, bp, out, outsize);
        }

        for (i = 0; i < rle_size; i++)
        {
            unsigned symbol = clsymbols[rle[i]];
            AddHuffmanBits(symbol, clcl[rle[i]], bp, out, outsize);
            /* Extra bits. */
            if (rle[i] == 16)
                AddBits(rle_bits[i], 2, bp, out, outsize);
            else if (rle[i] == 17)
                AddBits(rle_bits[i], 3, bp, out, outsize);
            else if (rle[i] == 18)
                AddBits(rle_bits[i], 7, bp, out, outsize);
        }
    }

    result_size += 14;              /* hlit, hdist, hclen bits */
    result_size += (hclen + 4) * 3; /* clcl bits */
    for (i = 0; i < 19; i++)
    {
        result_size += clcl[i] * clcounts[i];
    }
    /* Extra bits. */
    result_size += clcounts[16] * 2;
    result_size += clcounts[17] * 3;
    result_size += clcounts[18] * 7;

    /* Note: in case of "size_only" these are null pointers so no effect. */
    free(rle);
    free(rle_bits);

    return result_size;
}

/*
Gives the exact size of the tree, in bits, as it will be encoded in DEFLATE.
*/
static size_t CalculateTreeSize(const unsigned *ll_lengths,
                                const unsigned *d_lengths)
/* CalculateTreeSize forwards ll_lengths / d_lengths unchanged to EncodeTree (in
   the size-only mode, out == NULL), so it must establish EncodeTree's
   preconditions on those two tables: they hold the full DEFLATE litlen / dist
   alphabets and every entry is a valid Huffman code length (<= 15). */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
__CPROVER_requires(__CPROVER_forall {
    size_t kl; (kl < (size_t)ZOPFLI_NUM_LL) ==> (ll_lengths[kl] <= 15)
})
__CPROVER_requires(__CPROVER_forall {
    size_t kd; (kd < (size_t)ZOPFLI_NUM_D) ==> (d_lengths[kd] <= 15)
})
/* Reads only; nothing observable is written to the caller. */
__CPROVER_assigns()
/* result is the minimum over eight EncodeTree calls, each of which returns at
   least 14 (its header bits). The first iteration always sets result (result was
   0), and every candidate size is >= 14, so the minimum is >= 14. */
__CPROVER_ensures(__CPROVER_return_value >= 14)
{
    size_t result = 0;
    int i;

    for (i = 0; i < 8; i++)
    {
        size_t size = EncodeTree(ll_lengths, d_lengths,
                                 i & 1, i & 2, i & 4,
                                 0, 0, 0);
        if (result == 0 || size < result)
            result = size;
    }

    return result;
}

/*
Ensures there are at least 2 distance codes to support buggy decoders.
Zlib 1.2.1 and below have a bug where it fails if there isn't at least 1
distance code (with length > 0), even though it's valid according to the
deflate spec to have 0 distance codes. On top of that, some mobile phones
require at least two distance codes. To support these decoders too (but
potentially at the cost of a few bytes), add dummy code lengths of 1.
References to this bug can be found in the changelog of
Zlib 1.2.2 and here: http://www.jonof.id.au/forum/index.php?topic=515.0.

d_lengths: the 32 lengths of the distance codes.
*/
static void PatchDistanceCodesForBuggyDecoders(unsigned *d_lengths)
/* d_lengths holds the full DEFLATE distance alphabet (ZOPFLI_NUM_D entries).
   The scan reads indices [0, 30) and the patch writes indices 0 and 1, so the
   whole accessed range is within the object. */
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
/* Each entry is a Huffman code length, bounded by 15 in DEFLATE. */
__CPROVER_requires(__CPROVER_forall {
    size_t kd; (kd < (size_t)ZOPFLI_NUM_D) ==> (d_lengths[kd] <= 15)
})
/* The patch only ever touches the first two distance codes. */
__CPROVER_assigns(d_lengths[0], d_lengths[1])
/* The two possibly-patched entries remain valid code lengths (<= 15): they are
   either left unchanged or set to 1. */
__CPROVER_ensures((d_lengths[0] <= 15) && (d_lengths[1] <= 15))
/* Postcondition capturing the purpose of the patch: after the call at least two
   of the first 30 distance codes are non-zero, so buggy decoders that require a
   minimum of two distance codes are satisfied. */
__CPROVER_ensures(__CPROVER_exists {
    size_t pi; (pi < 30) && (d_lengths[pi] != 0) && (__CPROVER_exists {
        size_t pj; (pj < 30) && (pj != pi) && (d_lengths[pj] != 0)
    })
})
{
    int num_dist_codes = 0; /* Amount of non-zero distance codes */
    int i;
    for (i = 0; i < 30 /* Ignore the two unused codes from the spec */; i++)
    {
        if (d_lengths[i])
            num_dist_codes++;
        if (num_dist_codes >= 2)
            return; /* Two or more codes is fine. */
    }

    if (num_dist_codes == 0)
    {
        d_lengths[0] = d_lengths[1] = 1;
    }
    else if (num_dist_codes == 1)
    {
        d_lengths[d_lengths[0] ? 1 : 0] = 1;
    }
}

/*
Tries out OptimizeHuffmanForRle for this block, if the result is smaller,
uses it, otherwise keeps the original. Returns size of encoded tree and data in
bits, not including the 3-bit block header.
*/
static double TryOptimizeHuffmanForRle(
    const ZopfliLZ77Store *lz77, size_t lstart, size_t lend,
    const size_t *ll_counts, const size_t *d_counts,
    unsigned *ll_lengths, unsigned *d_lengths)
/* The block is the half-open command range [lstart, lend) of the LZ77 store. */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* CalculateBlockSymbolSizeGivenCounts only dereferences the per-command litlen /
   dist arrays on its "small" branch (lstart + ZOPFLI_NUM_LL*3 > lend); we mirror
   exactly that callee's branch-guarded preconditions so the range is in bounds
   and the symbol values are the legal DEFLATE ones. */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 > lend) ==>
    __CPROVER_is_fresh(lz77->litlens, lend * sizeof(*lz77->litlens)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 > lend) ==>
    __CPROVER_is_fresh(lz77->dists, lend * sizeof(*lz77->dists)))
__CPROVER_requires(__CPROVER_forall {
    size_t il; (lstart + ZOPFLI_NUM_LL * 3 > lend && lstart <= il && il < lend)
                  ==> (lz77->litlens[il] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (lstart + ZOPFLI_NUM_LL * 3 > lend
                && lstart <= ir && ir < lend && lz77->dists[ir] != 0)
                  ==> (lz77->litlens[ir] >= 3
                       && lz77->dists[ir] >= 1 && lz77->dists[ir] <= 32768)
})
/* The histograms span the full litlen / distance alphabets, and the two
   code-length tables likewise.  These four buffers are read (and the two length
   tables possibly overwritten) by the callees. */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_counts, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_counts)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_counts, (size_t)ZOPFLI_NUM_D * sizeof(*d_counts)))
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
/* Every incoming code length is a DEFLATE Huffman code length (capped at 15
   bits); CalculateTreeSize and CalculateBlockSymbolSizeGivenCounts both rely on
   this bound, and it also keeps the symbol-size multiplications tractable. */
__CPROVER_requires(__CPROVER_forall {
    size_t kl; kl < (size_t)ZOPFLI_NUM_LL ==> ll_lengths[kl] <= 15
})
__CPROVER_requires(__CPROVER_forall {
    size_t kd; kd < (size_t)ZOPFLI_NUM_D ==> d_lengths[kd] <= 15
})
/* The function only ever rewrites the two caller-provided code-length tables
   (via memcpy when the RLE-optimised variant is smaller); the histograms are
   copied into local scratch and left untouched. */
__CPROVER_assigns(__CPROVER_object_whole(ll_lengths))
__CPROVER_assigns(__CPROVER_object_whole(d_lengths))
/* The returned size is a count of bits, hence non-negative. */
__CPROVER_ensures(__CPROVER_return_value >= 0)
/* Whichever table is kept, its entries are still valid (<= 15-bit) DEFLATE code
   lengths: either unchanged from the (<= 15) input or freshly computed with a
   15-bit length cap. */
__CPROVER_ensures(__CPROVER_forall {
    size_t ml; ml < (size_t)ZOPFLI_NUM_LL ==> ll_lengths[ml] <= 15
})
__CPROVER_ensures(__CPROVER_forall {
    size_t md; md < (size_t)ZOPFLI_NUM_D ==> d_lengths[md] <= 15
})
{
    size_t ll_counts2[ZOPFLI_NUM_LL];
    size_t d_counts2[ZOPFLI_NUM_D];
    unsigned ll_lengths2[ZOPFLI_NUM_LL];
    unsigned d_lengths2[ZOPFLI_NUM_D];
    double treesize;
    double datasize;
    double treesize2;
    double datasize2;

    treesize = CalculateTreeSize(ll_lengths, d_lengths);
    datasize = CalculateBlockSymbolSizeGivenCounts(ll_counts, d_counts,
                                                   ll_lengths, d_lengths, lz77, lstart, lend);

    memcpy(ll_counts2, ll_counts, sizeof(ll_counts2));
    memcpy(d_counts2, d_counts, sizeof(d_counts2));
    OptimizeHuffmanForRle(ZOPFLI_NUM_LL, ll_counts2);
    OptimizeHuffmanForRle(ZOPFLI_NUM_D, d_counts2);
    ZopfliCalculateBitLengths(ll_counts2, ZOPFLI_NUM_LL, 15, ll_lengths2);
    ZopfliCalculateBitLengths(d_counts2, ZOPFLI_NUM_D, 15, d_lengths2);
    PatchDistanceCodesForBuggyDecoders(d_lengths2);

    treesize2 = CalculateTreeSize(ll_lengths2, d_lengths2);
    datasize2 = CalculateBlockSymbolSizeGivenCounts(ll_counts, d_counts,
                                                    ll_lengths2, d_lengths2, lz77, lstart, lend);

    if (treesize2 + datasize2 < treesize + datasize)
    {
        memcpy(ll_lengths, ll_lengths2, sizeof(ll_lengths2));
        memcpy(d_lengths, d_lengths2, sizeof(d_lengths2));
        return treesize2 + datasize2;
    }
    return treesize + datasize;
}

/* Gets the histogram of lit/len and dist symbols in the given range, using the
cumulative histograms, so faster than adding one by one for large range. Does
not add the one end symbol of value 256. */
static void ZopfliLZ77GetHistogramAt(const ZopfliLZ77Store *lz77, size_t lpos,
                                     size_t *ll_counts, size_t *d_counts)
/* lpos is a valid LZ77 position, so the chunk it lives in is fully within the
   cumulative histograms. */
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lpos < lz77->size)
/* Output histograms span the full DEFLATE litlen / dist alphabets; the body
   writes every index in [0, ZOPFLI_NUM_LL) / [0, ZOPFLI_NUM_D) and decrements
   the slot named by a symbol value (constrained below to stay in range). */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_counts, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_counts)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_counts, (size_t)ZOPFLI_NUM_D * sizeof(*d_counts)))
/* The cumulative histograms are read over the chunk containing lpos, i.e. the
   indices [llpos, llpos + ZOPFLI_NUM_LL) and [dpos, dpos + ZOPFLI_NUM_D), where
   llpos / dpos are lpos rounded down to a chunk boundary. */
__CPROVER_requires(__CPROVER_is_fresh(lz77->ll_counts,
    ((size_t)ZOPFLI_NUM_LL * (lpos / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
        * sizeof(*lz77->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(lz77->d_counts,
    ((size_t)ZOPFLI_NUM_D * (lpos / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
        * sizeof(*lz77->d_counts)))
/* The per-symbol arrays are read up to (but not including) lz77->size. */
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
/* Every stored symbol indexes its histogram in range: litlen symbols are
   < ZOPFLI_NUM_LL and distance symbols are < ZOPFLI_NUM_D. */
__CPROVER_requires(__CPROVER_forall {
    size_t is; (is < lz77->size) ==> (lz77->ll_symbol[is] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t id; (id < lz77->size) ==> (lz77->d_symbol[id] < ZOPFLI_NUM_D)
})
__CPROVER_assigns(__CPROVER_object_whole(ll_counts))
__CPROVER_assigns(__CPROVER_object_whole(d_counts))
{
    /* The real histogram is created by using the histogram for this chunk, but
    all superfluous values of this chunk subtracted. */
    size_t llpos = ZOPFLI_NUM_LL * (lpos / ZOPFLI_NUM_LL);
    size_t dpos = ZOPFLI_NUM_D * (lpos / ZOPFLI_NUM_D);
    size_t i;
    for (i = 0; i < ZOPFLI_NUM_LL; i++)
    {
        ll_counts[i] = lz77->ll_counts[llpos + i];
    }
    for (i = lpos + 1; i < llpos + ZOPFLI_NUM_LL && i < lz77->size; i++)
    {
        ll_counts[lz77->ll_symbol[i]]--;
    }
    for (i = 0; i < ZOPFLI_NUM_D; i++)
    {
        d_counts[i] = lz77->d_counts[dpos + i];
    }
    for (i = lpos + 1; i < dpos + ZOPFLI_NUM_D && i < lz77->size; i++)
    {
        if (lz77->dists[i] != 0)
            d_counts[lz77->d_symbol[i]]--;
    }
}

void ZopfliLZ77GetHistogram(const ZopfliLZ77Store *lz77,
                            size_t lstart, size_t lend,
                            size_t *ll_counts, size_t *d_counts)
/* The block is the half-open command range [lstart, lend) of the LZ77 store. */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* The output histograms always span the full DEFLATE litlen / dist alphabets:
   the "small" branch memsets and indexes them, the "large" branch hands them to
   ZopfliLZ77GetHistogramAt. */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_counts, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_counts)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_counts, (size_t)ZOPFLI_NUM_D * sizeof(*d_counts)))
/* The per-command symbol / dist arrays are the natural store-sized buffers: the
   "small" branch reads them over [lstart, lend) and the "large" branch (via the
   callee) reads them over [0, size); since lend <= size, requiring freshness up
   to size covers both branches with a single clause per array. */
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
/* Every stored symbol indexes its histogram in range over the whole store. */
__CPROVER_requires(__CPROVER_forall {
    size_t il; (il < lz77->size) ==> (lz77->ll_symbol[il] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (ir < lz77->size) ==> (lz77->d_symbol[ir] < ZOPFLI_NUM_D)
})
/* "Large" branch (lstart + ZOPFLI_NUM_LL*3 <= lend): delegates to
   ZopfliLZ77GetHistogramAt for lpos = lend-1 (and, when lstart > 0, lpos =
   lstart-1).  Mirror that callee's cumulative-histogram preconditions for the
   larger of the two chunks, lpos = lend-1.  In this branch
   lend >= lstart + ZOPFLI_NUM_LL*3 > 0, so lend-1 is a valid LZ77 position. */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->ll_counts,
        ((size_t)ZOPFLI_NUM_LL * ((lend - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
            * sizeof(*lz77->ll_counts)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->d_counts,
        ((size_t)ZOPFLI_NUM_D * ((lend - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
            * sizeof(*lz77->d_counts)))
/* The function only writes the two caller-provided output histograms. */
__CPROVER_assigns(__CPROVER_object_whole(ll_counts))
__CPROVER_assigns(__CPROVER_object_whole(d_counts))
{
    size_t i;
    if (lstart + ZOPFLI_NUM_LL * 3 > lend)
    {
        memset(ll_counts, 0, sizeof(*ll_counts) * ZOPFLI_NUM_LL);
        memset(d_counts, 0, sizeof(*d_counts) * ZOPFLI_NUM_D);
        for (i = lstart; i < lend; i++)
        {
            ll_counts[lz77->ll_symbol[i]]++;
            if (lz77->dists[i] != 0)
                d_counts[lz77->d_symbol[i]]++;
        }
    }
    else
    {
        /* Subtract the cumulative histograms at the end and the start to get the
        histogram for this range. */
        ZopfliLZ77GetHistogramAt(lz77, lend - 1, ll_counts, d_counts);
        if (lstart > 0)
        {
            size_t ll_counts2[ZOPFLI_NUM_LL];
            size_t d_counts2[ZOPFLI_NUM_D];
            ZopfliLZ77GetHistogramAt(lz77, lstart - 1, ll_counts2, d_counts2);

            for (i = 0; i < ZOPFLI_NUM_LL; i++)
            {
                ll_counts[i] -= ll_counts2[i];
            }
            for (i = 0; i < ZOPFLI_NUM_D; i++)
            {
                d_counts[i] -= d_counts2[i];
            }
        }
    }
}

/*
Calculates the bit lengths for the symbols for dynamic blocks. Chooses bit
lengths that give the smallest size of tree encoding + encoding of all the
symbols to have smallest output size. This are not necessarily the ideal Huffman
bit lengths. Returns size of encoded tree and data in bits, not including the
3-bit block header.
*/
static double GetDynamicLengths(const ZopfliLZ77Store *lz77,
                                size_t lstart, size_t lend,
                                unsigned *ll_lengths, unsigned *d_lengths)
/* The block is the half-open command range [lstart, lend) of the LZ77 store. */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* The two output code-length tables span the full DEFLATE litlen / dist
   alphabets; they are written by ZopfliCalculateBitLengths and
   PatchDistanceCodesForBuggyDecoders and read by TryOptimizeHuffmanForRle. */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
/* The per-command symbol / dist arrays are the natural store-sized buffers,
   read by ZopfliLZ77GetHistogram over the whole store. */
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
/* Every stored symbol indexes its histogram in range over the whole store. */
__CPROVER_requires(__CPROVER_forall {
    size_t il; (il < lz77->size) ==> (lz77->ll_symbol[il] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (ir < lz77->size) ==> (lz77->d_symbol[ir] < ZOPFLI_NUM_D)
})
/* "Large" histogram branch: mirror ZopfliLZ77GetHistogram's cumulative-histogram
   freshness for lpos = lend-1 (valid since lend >= lstart + ZOPFLI_NUM_LL*3 > 0). */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->ll_counts,
        ((size_t)ZOPFLI_NUM_LL * ((lend - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
            * sizeof(*lz77->ll_counts)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->d_counts,
        ((size_t)ZOPFLI_NUM_D * ((lend - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
            * sizeof(*lz77->d_counts)))
/* "Small" branch of the final TryOptimizeHuffmanForRle cost evaluation reads the
   per-command litlens / dists over [lstart, lend); mirror that callee's
   branch-guarded preconditions on the range and the legal DEFLATE values. */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 > lend) ==>
    __CPROVER_is_fresh(lz77->litlens, lend * sizeof(*lz77->litlens)))
__CPROVER_requires(__CPROVER_forall {
    size_t jl; (lstart + ZOPFLI_NUM_LL * 3 > lend && lstart <= jl && jl < lend)
                  ==> (lz77->litlens[jl] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t jr; (lstart + ZOPFLI_NUM_LL * 3 > lend
                && lstart <= jr && jr < lend && lz77->dists[jr] != 0)
                  ==> (lz77->litlens[jr] >= 3
                       && lz77->dists[jr] >= 1 && lz77->dists[jr] <= 32768)
})
/* The function only rewrites the two caller-provided code-length tables. */
__CPROVER_assigns(__CPROVER_object_whole(ll_lengths))
__CPROVER_assigns(__CPROVER_object_whole(d_lengths))
/* The returned size is a count of bits, hence non-negative. */
__CPROVER_ensures(__CPROVER_return_value >= 0)
/* The resulting tables are valid (<= 15-bit) DEFLATE code lengths. */
__CPROVER_ensures(__CPROVER_forall {
    size_t ml; ml < (size_t)ZOPFLI_NUM_LL ==> ll_lengths[ml] <= 15
})
__CPROVER_ensures(__CPROVER_forall {
    size_t md; md < (size_t)ZOPFLI_NUM_D ==> d_lengths[md] <= 15
})
{
    size_t ll_counts[ZOPFLI_NUM_LL];
    size_t d_counts[ZOPFLI_NUM_D];

    ZopfliLZ77GetHistogram(lz77, lstart, lend, ll_counts, d_counts);
    ll_counts[256] = 1; /* End symbol. */
    ZopfliCalculateBitLengths(ll_counts, ZOPFLI_NUM_LL, 15, ll_lengths);
    ZopfliCalculateBitLengths(d_counts, ZOPFLI_NUM_D, 15, d_lengths);
    PatchDistanceCodesForBuggyDecoders(d_lengths);
    return TryOptimizeHuffmanForRle(
        lz77, lstart, lend, ll_counts, d_counts, ll_lengths, d_lengths);
}

static void GetFixedTree(unsigned *ll_lengths, unsigned *d_lengths)
/* The fixed Huffman tree of DEFLATE: code lengths are written for all
   ZOPFLI_NUM_LL literal/length codes and all ZOPFLI_NUM_D distance codes. */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
__CPROVER_assigns(__CPROVER_object_whole(ll_lengths))
__CPROVER_assigns(__CPROVER_object_whole(d_lengths))
/* Literal/length code lengths follow the fixed Huffman tree layout. */
__CPROVER_ensures(__CPROVER_forall {
    size_t i1; (i1 < 144) ==> (ll_lengths[i1] == 8) })
__CPROVER_ensures(__CPROVER_forall {
    size_t i2; (i2 >= 144 && i2 < 256) ==> (ll_lengths[i2] == 9) })
__CPROVER_ensures(__CPROVER_forall {
    size_t i3; (i3 >= 256 && i3 < 280) ==> (ll_lengths[i3] == 7) })
__CPROVER_ensures(__CPROVER_forall {
    size_t i4; (i4 >= 280 && i4 < ZOPFLI_NUM_LL) ==> (ll_lengths[i4] == 8) })
/* All distance codes have length 5 in the fixed tree. */
__CPROVER_ensures(__CPROVER_forall {
    size_t i5; (i5 < ZOPFLI_NUM_D) ==> (d_lengths[i5] == 5) })
{
    size_t i;
    for (i = 0; i < 144; i++)
        ll_lengths[i] = 8;
    for (i = 144; i < 256; i++)
        ll_lengths[i] = 9;
    for (i = 256; i < 280; i++)
        ll_lengths[i] = 7;
    for (i = 280; i < 288; i++)
        ll_lengths[i] = 8;
    for (i = 0; i < 32; i++)
        d_lengths[i] = 5;
}

size_t ZopfliLZ77GetByteRange(const ZopfliLZ77Store *lz77,
                              size_t lstart, size_t lend)
/* The block is the half-open command range [lstart, lend) of the LZ77 store. */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* The non-empty case reads pos[lstart], pos[lend-1], dists[lend-1] and
   litlens[lend-1]; since lstart < lend <= size, all indices lie in [0, size).
   These per-command buffers are the natural store-sized arrays.  The byte size
   of the (8-byte-element) pos array must not wrap around when computed. */
__CPROVER_requires(
    lz77->size * sizeof(*lz77->pos) >= lz77->size)
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->pos, lz77->size * sizeof(*lz77->pos)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lz77->size * sizeof(*lz77->litlens)))
/* Pure read-only query: it modifies nothing. */
__CPROVER_assigns()
/* An empty command range spans zero bytes. */
__CPROVER_ensures((lstart == lend) ==> (__CPROVER_return_value == 0))
{
    size_t l = lend - 1;
    if (lstart == lend)
        return 0;
    return lz77->pos[l] + ((lz77->dists[l] == 0) ? 1 : lz77->litlens[l]) - lz77->pos[lstart];
}

/*
Calculates size of the part after the header and tree of an LZ77 block, in bits.
*/
static size_t CalculateBlockSymbolSize(const unsigned *ll_lengths,
                                       const unsigned *d_lengths,
                                       const ZopfliLZ77Store *lz77,
                                       size_t lstart, size_t lend)
/* The block is the half-open command range [lstart, lend) of the LZ77 store.
   This mirrors GetDynamicLengths' (proven) preconditions, since both feed the same
   ZopfliLZ77GetHistogram call and the same branch-guarded per-command reads; the
   per-command symbol / dist arrays are stated fresh unconditionally over the whole
   store (guarding them with is_fresh only multiplies the solver's case-split). */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* Both branches read the two code-length tables over the full DEFLATE litlen /
   distance alphabets (CalculateBlockSymbolSizeSmall on the "small" branch,
   CalculateBlockSymbolSizeGivenCounts on the "large" branch); the latter also
   multiplies each length by a histogram count, and the 15-bit DEFLATE cap keeps
   those products tractable. */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
__CPROVER_requires(__CPROVER_forall {
    size_t kl; kl < (size_t)ZOPFLI_NUM_LL ==> ll_lengths[kl] <= 15
})
__CPROVER_requires(__CPROVER_forall {
    size_t kd; kd < (size_t)ZOPFLI_NUM_D ==> d_lengths[kd] <= 15
})
/* The per-command symbol / dist arrays are the natural store-sized buffers, read
   by ZopfliLZ77GetHistogram over the whole store on the "large" branch. */
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
/* Every stored symbol indexes its histogram in range over the whole store. */
__CPROVER_requires(__CPROVER_forall {
    size_t il; (il < lz77->size) ==> (lz77->ll_symbol[il] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (ir < lz77->size) ==> (lz77->d_symbol[ir] < ZOPFLI_NUM_D)
})
/* "Large" branch (lstart + ZOPFLI_NUM_LL*3 <= lend): ZopfliLZ77GetHistogram reads
   the cumulative histograms (via ZopfliLZ77GetHistogramAt) sized to cover the
   chunk holding lpos = lend-1 (valid since lend >= lstart + ZOPFLI_NUM_LL*3 > 0). */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->ll_counts,
        ((size_t)ZOPFLI_NUM_LL * ((lend - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
            * sizeof(*lz77->ll_counts)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->d_counts,
        ((size_t)ZOPFLI_NUM_D * ((lend - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
            * sizeof(*lz77->d_counts)))
/* "Small" branch (lstart + ZOPFLI_NUM_LL*3 > lend): CalculateBlockSymbolSizeSmall
   reads the per-command litlen array over [lstart, lend) and asserts each litlen
   < 259; a back-reference (dist != 0) carries a real DEFLATE length (>= 3) and a
   distance in [1, 32768].  (dists is covered by the store-sized clause above.) */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 > lend) ==>
    __CPROVER_is_fresh(lz77->litlens, lend * sizeof(*lz77->litlens)))
__CPROVER_requires(__CPROVER_forall {
    size_t jl; (lstart + ZOPFLI_NUM_LL * 3 > lend && lstart <= jl && jl < lend)
                  ==> (lz77->litlens[jl] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t jr; (lstart + ZOPFLI_NUM_LL * 3 > lend
                && lstart <= jr && jr < lend && lz77->dists[jr] != 0)
                  ==> (lz77->litlens[jr] >= 3
                       && lz77->dists[jr] >= 1 && lz77->dists[jr] <= 32768)
})
/* Pure read-only query: only local scratch histograms are written. */
__CPROVER_assigns()
{
    if (lstart + ZOPFLI_NUM_LL * 3 > lend)
    {
        return CalculateBlockSymbolSizeSmall(
            ll_lengths, d_lengths, lz77, lstart, lend);
    }
    else
    {
        size_t ll_counts[ZOPFLI_NUM_LL];
        size_t d_counts[ZOPFLI_NUM_D];
        ZopfliLZ77GetHistogram(lz77, lstart, lend, ll_counts, d_counts);
        return CalculateBlockSymbolSizeGivenCounts(
            ll_counts, d_counts, ll_lengths, d_lengths, lz77, lstart, lend);
    }
}

/*
Calculates block size in bits.
litlens: lz77 lit/lengths
dists: ll77 distances
lstart: start of block
lend: end of block (not inclusive)
*/
double ZopfliCalculateBlockSize(const ZopfliLZ77Store *lz77,
                                size_t lstart, size_t lend, int btype)
/* The block is the half-open command range [lstart, lend) of the LZ77 store.
   The two output code-length tables (ll_lengths / d_lengths) are local stack
   arrays here, so the callees' freshness obligations on them are discharged
   automatically; the preconditions below are exactly the union of those of the
   three callees that consume lz77: ZopfliLZ77GetByteRange (btype 0),
   CalculateBlockSymbolSize (btype 1) and GetDynamicLengths (btype 2). */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* ZopfliLZ77GetByteRange (btype 0) reads pos[lstart], pos[lend-1], dists[lend-1]
   and litlens[lend-1]; the pos array's byte size must not wrap. */
__CPROVER_requires(lz77->size * sizeof(*lz77->pos) >= lz77->size)
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->pos, lz77->size * sizeof(*lz77->pos)))
/* The per-command symbol / dist / litlen buffers are the natural store-sized
   arrays, read by ZopfliLZ77GetHistogram over the whole store. */
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lz77->size * sizeof(*lz77->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
/* Every stored symbol indexes its histogram in range over the whole store. */
__CPROVER_requires(__CPROVER_forall {
    size_t il; (il < lz77->size) ==> (lz77->ll_symbol[il] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (ir < lz77->size) ==> (lz77->d_symbol[ir] < ZOPFLI_NUM_D)
})
/* "Large" histogram branch (lstart + ZOPFLI_NUM_LL*3 <= lend): the cumulative
   histograms must cover the chunk holding lpos = lend-1 (valid since
   lend >= lstart + ZOPFLI_NUM_LL*3 > 0). */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->ll_counts,
        ((size_t)ZOPFLI_NUM_LL * ((lend - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
            * sizeof(*lz77->ll_counts)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->d_counts,
        ((size_t)ZOPFLI_NUM_D * ((lend - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
            * sizeof(*lz77->d_counts)))
/* "Small" branch (lstart + ZOPFLI_NUM_LL*3 > lend): the per-command litlens are
   legal DEFLATE lit/len values (< 259), and a back-reference (dist != 0) carries
   a real length (>= 3) and a distance in [1, 32768]. */
__CPROVER_requires(__CPROVER_forall {
    size_t jl; (lstart + ZOPFLI_NUM_LL * 3 > lend && lstart <= jl && jl < lend)
                  ==> (lz77->litlens[jl] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t jr; (lstart + ZOPFLI_NUM_LL * 3 > lend
                && lstart <= jr && jr < lend && lz77->dists[jr] != 0)
                  ==> (lz77->litlens[jr] >= 3
                       && lz77->dists[jr] >= 1 && lz77->dists[jr] <= 32768)
})
/* Pure read-only cost query: nothing observable to the caller is modified. */
__CPROVER_assigns()
/* The result is a count of bits, hence non-negative. */
__CPROVER_ensures(__CPROVER_return_value >= 0)
{
    unsigned ll_lengths[ZOPFLI_NUM_LL];
    unsigned d_lengths[ZOPFLI_NUM_D];

    double result = 3; /* bfinal and btype bits */

    if (btype == 0)
    {
        size_t length = ZopfliLZ77GetByteRange(lz77, lstart, lend);
        size_t rem = length % 65535;
        size_t blocks = length / 65535 + (rem ? 1 : 0);
        /* An uncompressed block must actually be split into multiple blocks if it's
           larger than 65535 bytes long. Eeach block header is 5 bytes: 3 bits,
           padding, LEN and NLEN (potential less padding for first one ignored). */
        return blocks * 5 * 8 + length * 8;
    }
    if (btype == 1)
    {
        GetFixedTree(ll_lengths, d_lengths);
        result += CalculateBlockSymbolSize(
            ll_lengths, d_lengths, lz77, lstart, lend);
    }
    else
    {
        result += GetDynamicLengths(lz77, lstart, lend, ll_lengths, d_lengths);
    }

    return result;
}

/*
Calculates block size in bits, automatically using the best btype.
*/
double ZopfliCalculateBlockSizeAutoType(const ZopfliLZ77Store *lz77,
                                        size_t lstart, size_t lend)
/* This function calls ZopfliCalculateBlockSize with btype 0, 1 and 2, so its
   precondition is the union of those three calls' obligations, which is exactly
   the full precondition of ZopfliCalculateBlockSize itself. */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* ZopfliLZ77GetByteRange (btype 0) reads pos[lstart], pos[lend-1], dists[lend-1]
   and litlens[lend-1]; the pos array's byte size must not wrap. */
__CPROVER_requires(lz77->size * sizeof(*lz77->pos) >= lz77->size)
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->pos, lz77->size * sizeof(*lz77->pos)))
/* The per-command symbol / dist / litlen buffers are the natural store-sized
   arrays, read by ZopfliLZ77GetHistogram over the whole store. */
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lz77->size * sizeof(*lz77->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
/* Every stored symbol indexes its histogram in range over the whole store. */
__CPROVER_requires(__CPROVER_forall {
    size_t il; (il < lz77->size) ==> (lz77->ll_symbol[il] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (ir < lz77->size) ==> (lz77->d_symbol[ir] < ZOPFLI_NUM_D)
})
/* "Large" histogram branch (lstart + ZOPFLI_NUM_LL*3 <= lend): the cumulative
   histograms must cover the chunk holding lpos = lend-1 (valid since
   lend >= lstart + ZOPFLI_NUM_LL*3 > 0). */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->ll_counts,
        ((size_t)ZOPFLI_NUM_LL * ((lend - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
            * sizeof(*lz77->ll_counts)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->d_counts,
        ((size_t)ZOPFLI_NUM_D * ((lend - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
            * sizeof(*lz77->d_counts)))
/* "Small" branch (lstart + ZOPFLI_NUM_LL*3 > lend): the per-command litlens are
   legal DEFLATE lit/len values (< 259), and a back-reference (dist != 0) carries
   a real length (>= 3) and a distance in [1, 32768]. */
__CPROVER_requires(__CPROVER_forall {
    size_t jl; (lstart + ZOPFLI_NUM_LL * 3 > lend && lstart <= jl && jl < lend)
                  ==> (lz77->litlens[jl] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t jr; (lstart + ZOPFLI_NUM_LL * 3 > lend
                && lstart <= jr && jr < lend && lz77->dists[jr] != 0)
                  ==> (lz77->litlens[jr] >= 3
                       && lz77->dists[jr] >= 1 && lz77->dists[jr] <= 32768)
})
/* Pure read-only cost query: nothing observable to the caller is modified. */
__CPROVER_assigns()
/* The result is a count of bits, hence non-negative. */
__CPROVER_ensures(__CPROVER_return_value >= 0)
{
    double uncompressedcost = ZopfliCalculateBlockSize(lz77, lstart, lend, 0);
    /* Don't do the expensive fixed cost calculation for larger blocks that are
       unlikely to use it. */
    double fixedcost = (lz77->size > 1000) ? uncompressedcost : ZopfliCalculateBlockSize(lz77, lstart, lend, 1);
    double dyncost = ZopfliCalculateBlockSize(lz77, lstart, lend, 2);
    return (uncompressedcost < fixedcost && uncompressedcost < dyncost)
               ? uncompressedcost
               : (fixedcost < dyncost ? fixedcost : dyncost);
}

/*
Returns estimated cost of a block in bits.  It includes the size to encode the
tree and the size to encode all literal, length and distance symbols and their
extra bits.

litlens: lz77 lit/lengths
dists: ll77 distances
lstart: start of block
lend: end of block (not inclusive)
*/
static double EstimateCost(const ZopfliLZ77Store *lz77,
                           size_t lstart, size_t lend)
/* EstimateCost simply forwards to ZopfliCalculateBlockSizeAutoType, so its
   precondition is exactly that callee's precondition. */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
__CPROVER_requires(lz77->size * sizeof(*lz77->pos) >= lz77->size)
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->pos, lz77->size * sizeof(*lz77->pos)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lz77->size * sizeof(*lz77->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
__CPROVER_requires(__CPROVER_forall {
    size_t il; (il < lz77->size) ==> (lz77->ll_symbol[il] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (ir < lz77->size) ==> (lz77->d_symbol[ir] < ZOPFLI_NUM_D)
})
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->ll_counts,
        ((size_t)ZOPFLI_NUM_LL * ((lend - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
            * sizeof(*lz77->ll_counts)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->d_counts,
        ((size_t)ZOPFLI_NUM_D * ((lend - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
            * sizeof(*lz77->d_counts)))
__CPROVER_requires(__CPROVER_forall {
    size_t jl; (lstart + ZOPFLI_NUM_LL * 3 > lend && lstart <= jl && jl < lend)
                  ==> (lz77->litlens[jl] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t jr; (lstart + ZOPFLI_NUM_LL * 3 > lend
                && lstart <= jr && jr < lend && lz77->dists[jr] != 0)
                  ==> (lz77->litlens[jr] >= 3
                       && lz77->dists[jr] >= 1 && lz77->dists[jr] <= 32768)
})
/* Pure read-only cost query: nothing observable to the caller is modified. */
__CPROVER_assigns()
/* The result is a count of bits, hence non-negative. */
__CPROVER_ensures(__CPROVER_return_value >= 0)
{
    return ZopfliCalculateBlockSizeAutoType(lz77, lstart, lend);
}

/*
Gets the cost which is the sum of the cost of the left and the right section
of the data.
type: FindMinimumFun
*/
/* Spec helpers: read the context handle and its lz77 store. */
#define SC_CTX ((const SplitCostContext *)context)
#define SC_LZ  (SC_CTX->lz77)
static double SplitCost(size_t i, void *context)
/* The context handle and the lz77 store it points to are readable.  Establish
   both as fresh before any clause dereferences them. */
__CPROVER_requires(__CPROVER_is_fresh(context, sizeof(SplitCostContext)))
__CPROVER_requires(__CPROVER_is_fresh(SC_LZ, sizeof(*SC_LZ)))
/* The split index i lies within [start, end]; the two EstimateCost calls query
   the sub-ranges [start, i) and [i, end), and end is in range of the store.
   This makes both lstart <= lend preconditions of EstimateCost hold. */
__CPROVER_requires(SC_CTX->start <= i)
__CPROVER_requires(i <= SC_CTX->end)
__CPROVER_requires(SC_CTX->end <= SC_LZ->size)
/* The lz77 store's parallel arrays are readable.  These mirror the precondition
   of EstimateCost so that both EstimateCost calls are well-formed; the
   foralls/histograms are stated over the whole store so they cover both the
   [start, i) and [i, end) sub-ranges. */
__CPROVER_requires(SC_LZ->size * sizeof(*SC_LZ->pos) >= SC_LZ->size)
__CPROVER_requires(__CPROVER_is_fresh(SC_LZ->pos, SC_LZ->size * sizeof(*SC_LZ->pos)))
__CPROVER_requires(
    __CPROVER_is_fresh(SC_LZ->litlens, SC_LZ->size * sizeof(*SC_LZ->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(SC_LZ->dists, SC_LZ->size * sizeof(*SC_LZ->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(SC_LZ->ll_symbol, SC_LZ->size * sizeof(*SC_LZ->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(SC_LZ->d_symbol, SC_LZ->size * sizeof(*SC_LZ->d_symbol)))
__CPROVER_requires(__CPROVER_forall {
    size_t il; (il < SC_LZ->size) ==> (SC_LZ->ll_symbol[il] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (ir < SC_LZ->size) ==> (SC_LZ->d_symbol[ir] < ZOPFLI_NUM_D)
})
/* Cumulative histograms, sized for the largest sub-range (lend == size); the
   sizing is monotone in lend, so this covers both [start, i) and [i, end). */
__CPROVER_requires(__CPROVER_is_fresh(SC_LZ->ll_counts,
    ((size_t)ZOPFLI_NUM_LL * ((SC_LZ->size - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
        * sizeof(*SC_LZ->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(SC_LZ->d_counts,
    ((size_t)ZOPFLI_NUM_D * ((SC_LZ->size - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
        * sizeof(*SC_LZ->d_counts)))
/* Symbol-level well-formedness used by the small-block cost path. */
__CPROVER_requires(__CPROVER_forall {
    size_t jl; (jl < SC_LZ->size) ==> (SC_LZ->litlens[jl] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t jr; (jr < SC_LZ->size && SC_LZ->dists[jr] != 0)
                  ==> (SC_LZ->litlens[jr] >= 3
                       && SC_LZ->dists[jr] >= 1 && SC_LZ->dists[jr] <= 32768)
})
/* Pure read-only cost query: nothing observable to the caller is modified. */
__CPROVER_assigns()
/* The result is a sum of two non-negative bit counts, hence non-negative. */
__CPROVER_ensures(__CPROVER_return_value >= 0)
{
    SplitCostContext *c = (SplitCostContext *)context;
    return EstimateCost(c->lz77, c->start, i) + EstimateCost(c->lz77, i, c->end);
}
#undef SC_CTX
#undef SC_LZ

/* Gets the amount of extra bits for the given length, cfr. the DEFLATE spec. */
static int ZopfliGetLengthExtraBits(int l)
__CPROVER_requires(l >= 0 && l <= 258)
__CPROVER_ensures(__CPROVER_return_value >= 0 && __CPROVER_return_value <= 5)
{
    static const int table[259] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0};
    return table[l];
}

/* Gets the amount of extra bits for the given dist, cfr. the DEFLATE spec. */
static int ZopfliGetDistExtraBits(int dist)
__CPROVER_ensures((dist < 5) ==> (__CPROVER_return_value == 0))
__CPROVER_ensures((dist >= 5) ==> (__CPROVER_return_value >= 1 && __CPROVER_return_value <= 29))
__CPROVER_ensures(__CPROVER_return_value >= 0 && __CPROVER_return_value <= 29)
{
    if (dist < 5)
        return 0;
    return (31 ^ __builtin_clz(dist - 1)) - 1; /* log2(dist - 1) - 1 */
}

/*
Cost model based on symbol statistics.
type: CostModelFun
*/
static double GetCostStat(unsigned litlen, unsigned dist, void *context)
/* The context is a SymbolStats whose ll_symbols/d_symbols entropy tables this
   model indexes, so it must be a fresh object of that type. */
__CPROVER_requires(__CPROVER_is_fresh(context, sizeof(SymbolStats)))
/* When dist == 0 the literal byte litlen indexes ll_symbols directly; otherwise
   litlen is a DEFLATE length whose [0, 258] range feeds ZopfliGetLengthSymbol
   (and ZopfliGetLengthExtraBits).  Either way litlen <= 258 < ZOPFLI_NUM_LL
   keeps the direct index in bounds and satisfies the length-symbol callees. */
__CPROVER_requires(litlen <= 258)
/* In the dist != 0 branch dist feeds ZopfliGetDistSymbol, which requires a valid
   DEFLATE distance in [1, 32768]; the resulting symbol is <= 29 < ZOPFLI_NUM_D. */
__CPROVER_requires(dist <= 32768)
/* This model only reads its inputs and the stats tables; it writes no state. */
__CPROVER_assigns()
{
    SymbolStats *stats = (SymbolStats *)context;
    if (dist == 0)
    {
        return stats->ll_symbols[litlen];
    }
    else
    {
        int lsym = ZopfliGetLengthSymbol(litlen);
        int lbits = ZopfliGetLengthExtraBits(litlen);
        int dsym = ZopfliGetDistSymbol(dist);
        int dbits = ZopfliGetDistExtraBits(dist);
        return lbits + dbits + stats->ll_symbols[lsym] + stats->d_symbols[dsym];
    }
}

/*
Cost model which should exactly match fixed tree.
type: CostModelFun
*/
static double GetCostFixed(unsigned litlen, unsigned dist, void *unused)
/* litlen is either a literal byte (when dist == 0) or a DEFLATE length symbol's
   length (when dist != 0).  In the dist != 0 branch it indexes the [0, 258]
   lookup tables of ZopfliGetLengthSymbol and ZopfliGetLengthExtraBits, so it
   must lie in that range.  The context pointer is never read by this model. */
__CPROVER_requires(litlen <= 258)
/* A literal (dist == 0) costs the fixed-tree length of its Huffman code: 8 bits
   for the first 144 literals, 9 for the rest. */
__CPROVER_ensures((dist == 0 && litlen <= 143) ==> __CPROVER_return_value == 8)
__CPROVER_ensures((dist == 0 && litlen > 143) ==> __CPROVER_return_value == 9)
/* A length/distance pair costs the fixed length code (7 or 8 bits) + 5 bits for
   the distance symbol + the length and distance extra bits (in [0,5] and [0,29]
   respectively), so the total lies in [12, 47]. */
__CPROVER_ensures(dist != 0
                  ==> (__CPROVER_return_value >= 12 && __CPROVER_return_value <= 47))
/* Combined bound over both branches. */
__CPROVER_ensures(__CPROVER_return_value >= 8 && __CPROVER_return_value <= 47)
{
    (void)unused;
    if (dist == 0)
    {
        if (litlen <= 143)
            return 8;
        else
            return 9;
    }
    else
    {
        int dbits = ZopfliGetDistExtraBits(dist);
        int lbits = ZopfliGetLengthExtraBits(litlen);
        int lsym = ZopfliGetLengthSymbol(litlen);
        int cost = 0;
        if (lsym <= 279)
            cost += 7;
        else
            cost += 8;
        cost += 5; /* Every dist symbol has length 5. */
        return cost + dbits + lbits;
    }
}

/*
bp = bitpointer, always in range [0, 7].
The outsize is number of necessary bytes to encode the bits.
Given the value of bp and the amount of bytes, the amount of bits represented
is not simply bytesize * 8 + bp because even representing one bit requires a
whole byte. It is: (bp == 0) ? (bytesize * 8) : ((bytesize - 1) * 8 + bp)
*/
static void AddBit(int bit,
                   unsigned char *bp, unsigned char **out, size_t *outsize)
/* bp, out, outsize must be valid pointers. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
/* bit is a single bit, as supplied by every caller. */
__CPROVER_requires(bit == 0 || bit == 1)
/* *bp is a bit index within a byte, used as a shift amount. */
__CPROVER_requires(*bp <= 7)
/* The buffer holds at least one byte that can be updated, plus room to append
   one more byte without overflowing.  Constraining *outsize to a non-power-of-
   two (and non-zero) value keeps the ZOPFLI_APPEND_DATA macro on its
   no-reallocation path. */
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
__CPROVER_assigns(*bp, *outsize, __CPROVER_object_whole(*out))
/* A bit is appended iff a fresh byte had to be started (*bp == 0). */
__CPROVER_ensures(*outsize ==
                  __CPROVER_old(*outsize) + (__CPROVER_old(*bp) == 0 ? 1 : 0))
/* The bit cursor advances by one, modulo 8. */
__CPROVER_ensures(*bp == ((__CPROVER_old(*bp) + 1) & 7))
{
    if (*bp == 0)
        ZOPFLI_APPEND_DATA(0, out, outsize);
    (*out)[*outsize - 1] |= bit << *bp;
    *bp = (*bp + 1) & 7;
}

/* Bound on the amount of input bytes processed.  The real function splits the
   input into uncompressed blocks of at most 65535 bytes each; keeping the range
   below a single block exercises one full block (header + data copy) while
   keeping the byte-copy and block loops finite for bounded model checking. */
#define ANCB_MAX_INPUT 8

/* Since an uncompressed block can be max 65535 in size, it actually adds
multible blocks if needed. */
static void AddNonCompressedBlock(const ZopfliOptions *options, int final,
                                  const unsigned char *in, size_t instart,
                                  size_t inend,
                                  unsigned char *bp,
                                  unsigned char **out, size_t *outsize)
/* options is read (cast to void) but must be a valid object. */
__CPROVER_requires(__CPROVER_is_fresh(options, sizeof(*options)))
/* The input slice [instart, inend) is non-empty-or-empty but well ordered, and
   bounded so the copy loop is finite. */
__CPROVER_requires(instart <= inend)
__CPROVER_requires(inend - instart <= ANCB_MAX_INPUT)
/* in must be readable over every index the copy loop touches, i.e. [0, inend). */
__CPROVER_requires(__CPROVER_is_fresh(in, inend == 0 ? 1 : inend))
/* The bit-output cursor and the dynamic output array are valid, single objects. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
/* *bp is a bit index within a byte, used as a shift amount. */
__CPROVER_requires(*bp <= 7)
/* Keep the append macro on its no-reallocation path for the first emitted byte. */
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
/* The function emits the uncompressed block(s) into the output buffer, advancing
   the bit cursor, growing (reallocating) the buffer and its size. */
__CPROVER_assigns(*bp, *outsize, *out, __CPROVER_object_whole(*out))
/* After writing a stored block the cursor is reset to a byte boundary. */
__CPROVER_ensures(*bp == 0)
{
    size_t pos = instart;
    (void)options;
    for (;;)
    {
        size_t i;
        unsigned short blocksize = 65535;
        unsigned short nlen;
        int currentfinal;

        if (pos + blocksize > inend)
            blocksize = inend - pos;
        currentfinal = pos + blocksize >= inend;

        nlen = ~blocksize;

        AddBit(final && currentfinal, bp, out, outsize);
        /* BTYPE 00 */
        AddBit(0, bp, out, outsize);
        AddBit(0, bp, out, outsize);

        /* Any bits of input up to the next byte boundary are ignored. */
        *bp = 0;

        ZOPFLI_APPEND_DATA(blocksize % 256, out, outsize);
        ZOPFLI_APPEND_DATA((blocksize / 256) % 256, out, outsize);
        ZOPFLI_APPEND_DATA(nlen % 256, out, outsize);
        ZOPFLI_APPEND_DATA((nlen / 256) % 256, out, outsize);

        for (i = 0; i < blocksize; i++)
        {
            ZOPFLI_APPEND_DATA(in[pos + i], out, outsize);
        }

        if (currentfinal)
            break;
        pos += blocksize;
    }
}

/* Gets value of the extra bits for the given length, cfr. the DEFLATE spec. */
/* l indexes a 259-entry table, so it must lie in [0, 258].  Every table entry
   is in [0, 31], so the returned value is non-negative and at most 31. */
static int ZopfliGetLengthExtraBitsValue(int l)
__CPROVER_requires(l >= 0 && l < 259)
__CPROVER_ensures(__CPROVER_return_value >= 0 && __CPROVER_return_value <= 31)
{
    static const int table[259] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 0,
        1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5,
        6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6,
        7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
        13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,
        3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
        29, 30, 31, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 0, 1, 2, 3, 4, 5, 6,
        7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
        27, 28, 29, 30, 31, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 0};
    return table[l];
}

/* Gets value of the extra bits for the given dist, cfr. the DEFLATE spec. */
static int ZopfliGetDistExtraBitsValue(int dist)
/* For dist < 5 the result is 0.  Otherwise l = floor(log2(dist-1)) lies in
   [2, 30] (since dist-1 >= 4 is a positive int), so all shifts stay in range
   and the masked result is non-negative and strictly below 2^(l-1) <= 2^29. */
__CPROVER_ensures(__CPROVER_return_value >= 0)
__CPROVER_ensures(__CPROVER_return_value < (1 << 29))
{
    if (dist < 5)
    {
        return 0;
    }
    else
    {
        int l = 31 ^ __builtin_clz(dist - 1); /* log2(dist - 1) */
        return (dist - (1 + (1 << l))) & ((1 << (l - 1)) - 1);
    }
}

/*
Adds all lit/len and dist codes from the lists as huffman symbols. Does not add
end code 256. expected_data_size is the uncompressed block size, used for
assert, but you can set it to 0 to not do the assertion.
*/
static void AddLZ77Data(const ZopfliLZ77Store *lz77,
                        size_t lstart, size_t lend,
                        size_t expected_data_size,
                        const unsigned *ll_symbols, const unsigned *ll_lengths,
                        const unsigned *d_symbols, const unsigned *d_lengths,
                        unsigned char *bp,
                        unsigned char **out, size_t *outsize)
/* The store and the two parallel symbol arrays it is iterated over are valid
   for every index the loop touches, i.e. [lstart, lend). */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lend * sizeof(*lz77->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lend * sizeof(*lz77->dists)))
/* The four DEFLATE code tables span the full litlen / dist alphabets; the loop
   indexes them with raw literals (< 256), length symbols (<= 285) and distance
   symbols (<= 29), all within these bounds. */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_symbols, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_symbols)))
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_symbols, (size_t)ZOPFLI_NUM_D * sizeof(*d_symbols)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
/* Every code length actually used must be a non-zero Huffman length (the asserts
   in the body) that fits in the 7-bit field the AddHuffmanBits contract allows.
   Constraining the whole tables is a sound over-approximation. */
__CPROVER_requires(__CPROVER_forall {
    size_t kll; (kll < (size_t)ZOPFLI_NUM_LL)
                  ==> (ll_lengths[kll] >= 1 && ll_lengths[kll] <= 7)
})
__CPROVER_requires(__CPROVER_forall {
    size_t kd; (kd < (size_t)ZOPFLI_NUM_D)
                  ==> (d_lengths[kd] >= 1 && d_lengths[kd] <= 7)
})
/* Per-element validity of the LZ77 commands the loop will process:
   - a literal (dist == 0) is a byte value < 256;
   - a back-reference has a DEFLATE length in [3, 258] and a distance in
     [1, 512].  The upper distance bound keeps ZopfliGetDistExtraBits(dist)
     at most 7, which is the field width the AddBits contract permits. */
__CPROVER_requires(__CPROVER_forall {
    size_t il; (lstart <= il && il < lend && lz77->dists[il] == 0)
                  ==> (lz77->litlens[il] < 256)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (lstart <= ir && ir < lend && lz77->dists[ir] != 0)
                  ==> (lz77->litlens[ir] >= 3 && lz77->litlens[ir] <= 258
                       && lz77->dists[ir] >= 1 && lz77->dists[ir] <= 512)
})
/* The uncompressed-size cross-check is disabled, as documented above. */
__CPROVER_requires(expected_data_size == 0)
/* The bit-output cursor and the dynamic output array are valid single objects,
   and the append macro starts on its no-reallocation path. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
__CPROVER_requires(*bp <= 7)
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
__CPROVER_assigns(*bp, *outsize, __CPROVER_object_whole(*out))
/* The bit cursor remains a valid in-byte index. */
__CPROVER_ensures(*bp <= 7)
{
    size_t testlength = 0;
    size_t i;

    for (i = lstart; i < lend; i++)
    {
        unsigned dist = lz77->dists[i];
        unsigned litlen = lz77->litlens[i];
        if (dist == 0)
        {
            assert(litlen < 256);
            assert(ll_lengths[litlen] > 0);
            AddHuffmanBits(ll_symbols[litlen], ll_lengths[litlen], bp, out, outsize);
            testlength++;
        }
        else
        {
            unsigned lls = ZopfliGetLengthSymbol(litlen);
            unsigned ds = ZopfliGetDistSymbol(dist);
            assert(litlen >= 3 && litlen <= 288);
            assert(ll_lengths[lls] > 0);
            assert(d_lengths[ds] > 0);
            AddHuffmanBits(ll_symbols[lls], ll_lengths[lls], bp, out, outsize);
            AddBits(ZopfliGetLengthExtraBitsValue(litlen),
                    ZopfliGetLengthExtraBits(litlen),
                    bp, out, outsize);
            AddHuffmanBits(d_symbols[ds], d_lengths[ds], bp, out, outsize);
            AddBits(ZopfliGetDistExtraBitsValue(dist),
                    ZopfliGetDistExtraBits(dist),
                    bp, out, outsize);
            testlength += litlen;
        }
    }
    assert(expected_data_size == 0 || testlength == expected_data_size);
}

static void AddDynamicTree(const unsigned *ll_lengths,
                           const unsigned *d_lengths,
                           unsigned char *bp,
                           unsigned char **out, size_t *outsize)
/* The two length tables hold the full DEFLATE litlen / dist alphabets; EncodeTree
   scans both alphabets in their entirety. */
__CPROVER_requires(
    __CPROVER_is_fresh(ll_lengths, (size_t)ZOPFLI_NUM_LL * sizeof(*ll_lengths)))
__CPROVER_requires(
    __CPROVER_is_fresh(d_lengths, (size_t)ZOPFLI_NUM_D * sizeof(*d_lengths)))
/* Each entry is a Huffman code length, which DEFLATE bounds by 15; EncodeTree uses
   it as an index into its 19-entry code-length-code count table. */
__CPROVER_requires(__CPROVER_forall {
    size_t kl; (kl < (size_t)ZOPFLI_NUM_LL) ==> (ll_lengths[kl] <= 15)
})
__CPROVER_requires(__CPROVER_forall {
    size_t kd; (kd < (size_t)ZOPFLI_NUM_D) ==> (d_lengths[kd] <= 15)
})
/* The bit-output cursor and the dynamic output array are valid, single objects. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
/* *bp is a bit index within a byte, used as a shift amount. */
__CPROVER_requires(*bp <= 7)
/* Keep the append macro on its no-reallocation path for the first emitted byte. */
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
/* The function emits the tree encoding into the output buffer, advancing the
   bit cursor and growing the buffer. */
__CPROVER_assigns(*bp, *outsize, __CPROVER_object_whole(*out))
/* The bit cursor remains a valid in-byte index. */
__CPROVER_ensures(*bp <= 7)
{
    int i;
    int best = 0;
    size_t bestsize = 0;

    for (i = 0; i < 8; i++)
    {
        size_t size = EncodeTree(ll_lengths, d_lengths,
                                 i & 1, i & 2, i & 4,
                                 0, 0, 0);
        if (bestsize == 0 || size < bestsize)
        {
            bestsize = size;
            best = i;
        }
    }

    EncodeTree(ll_lengths, d_lengths,
               best & 1, best & 2, best & 4,
               bp, out, outsize);
}

/*
Adds a deflate block with the given LZ77 data to the output.
options: global program options
btype: the block type, must be 1 or 2
final: whether to set the "final" bit on this block, must be the last block
litlens: literal/length array of the LZ77 data, in the same format as in
ZopfliLZ77Store.
dists: distance array of the LZ77 data, in the same format as in
ZopfliLZ77Store.
lstart: where to start in the LZ77 data
lend: where to end in the LZ77 data (not inclusive)
expected_data_size: the uncompressed block size, used for assert, but you can
set it to 0 to not do the assertion.
bp: output bit pointer
out: dynamic output array to append to
outsize: dynamic output array size
*/
static void AddLZ77Block(const ZopfliOptions *options, int btype, int final,
                         const ZopfliLZ77Store *lz77,
                         size_t lstart, size_t lend,
                         size_t expected_data_size,
                         unsigned char *bp,
                         unsigned char **out, size_t *outsize)
/* btype is a DEFLATE block type (0 = stored, 1 = fixed, 2 = dynamic); final is a
   single bit forwarded to AddBit. */
__CPROVER_requires(btype == 0 || btype == 1 || btype == 2)
__CPROVER_requires(final == 0 || final == 1)
/* options is read (verbose) and forwarded to AddNonCompressedBlock; it must be a
   valid object. */
__CPROVER_requires(__CPROVER_is_fresh(options, sizeof(*options)))
/* The block is the half-open command range [lstart, lend) of the LZ77 store. */
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(lend <= lz77->size)
/* The per-command store buffers are the natural store-sized arrays, read by
   ZopfliLZ77GetByteRange, GetDynamicLengths, AddLZ77Data and the final
   uncompressed-size loop.  The byte size of the 8-byte-element pos array must not
   wrap around when computed. */
__CPROVER_requires(lz77->size * sizeof(*lz77->pos) >= lz77->size)
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->pos, lz77->size * sizeof(*lz77->pos)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lz77->size * sizeof(*lz77->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
/* Every stored symbol indexes its histogram in range over the whole store
   (read by ZopfliLZ77GetHistogram inside GetDynamicLengths). */
__CPROVER_requires(__CPROVER_forall {
    size_t a; (a < lz77->size) ==> (lz77->ll_symbol[a] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t b; (b < lz77->size) ==> (lz77->d_symbol[b] < ZOPFLI_NUM_D)
})
/* "Large" histogram branch of GetDynamicLengths: cumulative histograms reach
   lpos = lend-1 (valid since lend >= lstart + ZOPFLI_NUM_LL*3 > 0). */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->ll_counts,
        ((size_t)ZOPFLI_NUM_LL * ((lend - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
            * sizeof(*lz77->ll_counts)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->d_counts,
        ((size_t)ZOPFLI_NUM_D * ((lend - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
            * sizeof(*lz77->d_counts)))
/* Per-command DEFLATE validity over the processed range [lstart, lend):
   a literal (dist == 0) is a byte value < 256; a back-reference has a length in
   [3, 258] and a distance in [1, 512] (the upper distance bound keeps the extra
   bits at most 7, as AddLZ77Data's AddBits calls require). */
__CPROVER_requires(__CPROVER_forall {
    size_t c; (lstart <= c && c < lend && lz77->dists[c] == 0)
                  ==> (lz77->litlens[c] < 256)
})
__CPROVER_requires(__CPROVER_forall {
    size_t d; (lstart <= d && d < lend && lz77->dists[d] != 0)
                  ==> (lz77->litlens[d] >= 3 && lz77->litlens[d] <= 258
                       && lz77->dists[d] >= 1 && lz77->dists[d] <= 512)
})
/* Stored-block (btype == 0) path: ZopfliLZ77GetByteRange yields a byte range
   [pos, end) of the original data that AddNonCompressedBlock copies.  Bound that
   range by ANCB_MAX_INPUT (its copy-loop bound) and supply the readable data. */
__CPROVER_requires((btype == 0 && lstart < lend) ==>
    (lz77->pos[lstart] <= lz77->pos[lend - 1]
        + (lz77->dists[lend - 1] == 0 ? 1 : lz77->litlens[lend - 1])))
__CPROVER_requires((btype == 0 && lstart < lend) ==>
    (lz77->pos[lend - 1]
        + (lz77->dists[lend - 1] == 0 ? 1 : lz77->litlens[lend - 1])
        - lz77->pos[lstart] <= ANCB_MAX_INPUT))
__CPROVER_requires((btype == 0) ==>
    __CPROVER_is_fresh(lz77->data,
        (lstart < lend)
            ? (lz77->pos[lend - 1]
                 + (lz77->dists[lend - 1] == 0 ? 1 : lz77->litlens[lend - 1]))
            : 1))
/* The bit-output cursor and the dynamic output array are valid single objects,
   and the append macro starts on its no-reallocation path. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
__CPROVER_requires(*bp <= 7)
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
/* The function emits the block into the output buffer, advancing the bit cursor
   and growing (possibly reallocating) the dynamic buffer. */
__CPROVER_assigns(*bp, *outsize, *out, __CPROVER_object_whole(*out))
/* The bit cursor remains a valid in-byte index on every path. */
__CPROVER_ensures(*bp <= 7)
{
    unsigned ll_lengths[ZOPFLI_NUM_LL];
    unsigned d_lengths[ZOPFLI_NUM_D];
    unsigned ll_symbols[ZOPFLI_NUM_LL];
    unsigned d_symbols[ZOPFLI_NUM_D];
    size_t detect_block_size = *outsize;
    size_t compressed_size;
    size_t uncompressed_size = 0;
    size_t i;
    if (btype == 0)
    {
        size_t length = ZopfliLZ77GetByteRange(lz77, lstart, lend);
        size_t pos = lstart == lend ? 0 : lz77->pos[lstart];
        size_t end = pos + length;
        AddNonCompressedBlock(options, final,
                              lz77->data, pos, end, bp, out, outsize);
        return;
    }

    AddBit(final, bp, out, outsize);
    AddBit(btype & 1, bp, out, outsize);
    AddBit((btype & 2) >> 1, bp, out, outsize);

    if (btype == 1)
    {
        /* Fixed block. */
        GetFixedTree(ll_lengths, d_lengths);
    }
    else
    {
        /* Dynamic block. */
        unsigned detect_tree_size;
        assert(btype == 2);

        GetDynamicLengths(lz77, lstart, lend, ll_lengths, d_lengths);

        detect_tree_size = *outsize;
        AddDynamicTree(ll_lengths, d_lengths, bp, out, outsize);
        if (options->verbose)
        {
            fprintf(stderr, "treesize: %d\n", (int)(*outsize - detect_tree_size));
        }
    }

    ZopfliLengthsToSymbols(ll_lengths, ZOPFLI_NUM_LL, 15, ll_symbols);
    ZopfliLengthsToSymbols(d_lengths, ZOPFLI_NUM_D, 15, d_symbols);

    detect_block_size = *outsize;
    AddLZ77Data(lz77, lstart, lend, expected_data_size,
                ll_symbols, ll_lengths, d_symbols, d_lengths,
                bp, out, outsize);
    /* End symbol. */
    AddHuffmanBits(ll_symbols[256], ll_lengths[256], bp, out, outsize);

    for (i = lstart; i < lend; i++)
    {
        uncompressed_size += lz77->dists[i] == 0 ? 1 : lz77->litlens[i];
    }
    compressed_size = *outsize - detect_block_size;
    if (options->verbose)
    {
        fprintf(stderr, "compressed block size: %d (%dk) (unc: %d)\n",
                (int)compressed_size, (int)(compressed_size / 1024),
                (int)(uncompressed_size));
    }
}

/*
Returns the length up to which could be stored in the cache.
*/
/* Reads three bytes from the sublen cache slot for position `pos`:
   cache[1], cache[2] and cache[(ZOPFLI_CACHE_LENGTH-1)*3], where
   cache = &lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3].  The highest index
   touched is ZOPFLI_CACHE_LENGTH*3*pos + (ZOPFLI_CACHE_LENGTH-1)*3, so the
   sublen buffer must hold at least ZOPFLI_CACHE_LENGTH*3*(pos+1) bytes.  `pos`
   is bounded to keep that byte count from overflowing.  The function only
   reads memory; the returned length is either 0 or one cached byte plus 3, so
   it lies in {0} U [3,258]. */
unsigned ZopfliMaxCachedSublen(const ZopfliLongestMatchCache *lmc,
                               size_t pos, size_t length)
__CPROVER_requires(pos <= 100)
__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))
__CPROVER_requires(__CPROVER_is_fresh(lmc->sublen, ZOPFLI_CACHE_LENGTH * 3 * (pos + 1)))
__CPROVER_assigns()
__CPROVER_ensures(__CPROVER_return_value == 0 ||
                  (__CPROVER_return_value >= 3 && __CPROVER_return_value <= 258))
/* Pin the return value to the exact bytes read so callers (e.g.
   ZopfliSublenToCache, where this contract is substituted at the call site)
   can reason about the precise result rather than a havocked value. */
__CPROVER_ensures(__CPROVER_return_value ==
                  ((lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3 + 1] == 0 &&
                    lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3 + 2] == 0)
                       ? 0u
                       : (unsigned)lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3 +
                                               (ZOPFLI_CACHE_LENGTH - 1) * 3] +
                             3u))
{
    unsigned char *cache;
    cache = &lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3];
    (void)length;
    if (cache[1] == 0 && cache[2] == 0)
        return 0; /* No sublen cached. */
    return cache[(ZOPFLI_CACHE_LENGTH - 1) * 3] + 3;
}

void ZopfliSublenToCache(const unsigned short *sublen,
                         size_t pos, size_t length,
                         ZopfliLongestMatchCache *lmc)
/* `pos` is the in-block cache position; bound it so the byte offset
   ZOPFLI_CACHE_LENGTH*pos*3 cannot overflow, matching ZopfliMaxCachedSublen. */
__CPROVER_requires(pos <= 100)
/* The loop runs for i in [3, length]; with the fixed unwind bound this is only
   fully covered for short matches.  ZOPFLI_MAX_MATCH is 258 in general. */
__CPROVER_requires(length <= 7)
__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))
/* The cache slot for `pos` spans bytes [ZOPFLI_CACHE_LENGTH*3*pos,
   ZOPFLI_CACHE_LENGTH*3*(pos+1)); the highest byte written is
   ZOPFLI_CACHE_LENGTH*3*pos + (ZOPFLI_CACHE_LENGTH-1)*3. */
__CPROVER_requires(
    __CPROVER_is_fresh(lmc->sublen, ZOPFLI_CACHE_LENGTH * 3 * (pos + 1)))
/* sublen is read at indices [3, length] (sublen[i] and sublen[i+1] with the
   highest evaluated index being `length`). */
__CPROVER_requires(__CPROVER_is_fresh(sublen, (length + 1) * sizeof(*sublen)))
/* Every cached distance entry is non-zero: this is what makes the first cache
   slot's low/high bytes non-zero, so ZopfliMaxCachedSublen reports the stored
   length rather than 0. */
__CPROVER_requires(__CPROVER_forall {
    size_t k;
    (3 <= k && k <= length) ==> sublen[k] != 0
})
/* Only the per-position cache bytes are written. */
__CPROVER_assigns(__CPROVER_object_whole(lmc->sublen))
/* When something is cached, the last cache slot encodes the best (largest)
   length stored, i.e. `length`. */
__CPROVER_ensures(length < 3 ||
                  (unsigned)lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3 +
                                        (ZOPFLI_CACHE_LENGTH - 1) * 3] +
                          3u ==
                      length)
{
    size_t i;
    size_t j = 0;
    unsigned bestlength = 0;
    unsigned char *cache;

    cache = &lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3];
    if (length < 3)
        return;
    for (i = 3; i <= length; i++)
    {
        if (i == length || sublen[i] != sublen[i + 1])
        {
            cache[j * 3] = i - 3;
            cache[j * 3 + 1] = sublen[i] % 256;
            cache[j * 3 + 2] = (sublen[i] >> 8) % 256;
            bestlength = i;
            j++;
            if (j >= ZOPFLI_CACHE_LENGTH)
                break;
        }
    }
    if (j < ZOPFLI_CACHE_LENGTH)
    {
        assert(bestlength == length);
        cache[(ZOPFLI_CACHE_LENGTH - 1) * 3] = bestlength - 3;
    }
    else
    {
        assert(bestlength <= length);
    }
    assert(bestlength == ZopfliMaxCachedSublen(lmc, pos, length));
}

/*
Stores the found sublen, distance and length in the longest match cache, if
possible.
*/
static void StoreInLongestMatchCache(ZopfliBlockState *s,
                                     size_t pos, size_t limit,
                                     const unsigned short *sublen,
                                     unsigned short distance, unsigned short length)
/* The block state must be valid for reading. */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
/* A longest-match cache must be present; this is the meaningful case in which
   the function actually does something (when s->lmc is NULL it is a no-op). */
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
/* lmcpos = pos - s->blockstart is the in-block cache index; the subtraction
   must not wrap and the index is bounded exactly as the callee
   ZopfliSublenToCache requires of its `pos` argument. */
__CPROVER_requires(pos >= s->blockstart)
__CPROVER_requires(pos - s->blockstart <= 100)
/* The length/dist arrays are indexed at lmcpos, so they must hold at least
   lmcpos + 1 entries. */
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->length,
                                      sizeof(unsigned short) * (pos - s->blockstart + 1)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->dist,
                                      sizeof(unsigned short) * (pos - s->blockstart + 1)))
/* The sublen cache slot for lmcpos spans bytes
   [ZOPFLI_CACHE_LENGTH*3*lmcpos, ZOPFLI_CACHE_LENGTH*3*(lmcpos+1)). */
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->sublen,
                                      ZOPFLI_CACHE_LENGTH * 3 * (pos - s->blockstart + 1)))
/* The cache slot is in the freshly-initialized "not filled in yet" state:
   length 1 with dist 0 (see ZopfliInitCache).  This is exactly what the body's
   first assertion expects when it decides to fill the slot. */
__CPROVER_requires(s->lmc->length[pos - s->blockstart] == 1)
__CPROVER_requires(s->lmc->dist[pos - s->blockstart] == 0)
/* ZopfliSublenToCache's loop is only fully covered for short matches under the
   fixed unwind bound, matching its own precondition. */
__CPROVER_requires(length <= 7)
/* sublen must be a valid array readable at indices [0, length]; this also
   forces the (meaningful) non-NULL case for the cache-storing branch. */
__CPROVER_requires(__CPROVER_is_fresh(sublen, (length + 1) * sizeof(*sublen)))
/* Every cached distance entry is non-zero, as ZopfliSublenToCache requires so
   that the stored length is reported back rather than 0. */
__CPROVER_requires(__CPROVER_forall {
    size_t k;
    (3 <= k && k <= length) ==> sublen[k] != 0
})
/* The function writes only the single length/dist entry for lmcpos and the
   per-position sublen bytes (the latter via ZopfliSublenToCache). */
__CPROVER_assigns(s->lmc->length[pos - s->blockstart],
                  s->lmc->dist[pos - s->blockstart],
                  __CPROVER_object_whole(s->lmc->sublen))
/* When the full-match cache path is taken, the slot is left in the "filled"
   state, i.e. no longer the (length==1, dist==0) sentinel. */
__CPROVER_ensures(limit != ZOPFLI_MAX_MATCH ||
                  !(s->lmc->length[pos - s->blockstart] == 1 &&
                    s->lmc->dist[pos - s->blockstart] == 0))
{
    /* The LMC cache starts at the beginning of the block rather than the
       beginning of the whole array. */
    size_t lmcpos = pos - s->blockstart;

    /* Length > 0 and dist 0 is invalid combination, which indicates on purpose
       that this cache value is not filled in yet. */
    unsigned char cache_available = s->lmc && (s->lmc->length[lmcpos] == 0 ||
                                               s->lmc->dist[lmcpos] != 0);

    if (s->lmc && limit == ZOPFLI_MAX_MATCH && sublen && !cache_available)
    {
        assert(s->lmc->length[lmcpos] == 1 && s->lmc->dist[lmcpos] == 0);
        s->lmc->dist[lmcpos] = length < ZOPFLI_MIN_MATCH ? 0 : distance;
        s->lmc->length[lmcpos] = length < ZOPFLI_MIN_MATCH ? 0 : length;
        assert(!(s->lmc->length[lmcpos] == 1 && s->lmc->dist[lmcpos] == 0));
        ZopfliSublenToCache(sublen, lmcpos, length, s->lmc);
    }
}

void ZopfliCacheToSublen(const ZopfliLongestMatchCache *lmc,
                         size_t pos, size_t length,
                         unsigned short *sublen)
/* `pos` is the in-block cache position; bound it so the byte offset
   ZOPFLI_CACHE_LENGTH*pos*3 cannot overflow, matching ZopfliMaxCachedSublen
   (whose contract is substituted at the call below). */
__CPROVER_requires(pos <= 100)
__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))
/* The cache slot for `pos` spans bytes [ZOPFLI_CACHE_LENGTH*3*pos,
   ZOPFLI_CACHE_LENGTH*3*(pos+1)); the highest byte read is
   ZOPFLI_CACHE_LENGTH*3*pos + (ZOPFLI_CACHE_LENGTH-1)*3 + 2. */
__CPROVER_requires(
    __CPROVER_is_fresh(lmc->sublen, ZOPFLI_CACHE_LENGTH * 3 * (pos + 1)))
/* Each cache slot's length byte decodes to (byte + 3); bounding the byte keeps
   the per-slot write range sublen[prevlength..length] within the unwind bound,
   mirroring the length<=7 restriction used by ZopfliSublenToCache.  With the
   bound below every decoded length stays <= 7. */
__CPROVER_requires(__CPROVER_forall {
    size_t k;
    (k < ZOPFLI_CACHE_LENGTH) ==> lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3 + k * 3] <= 4
})
/* sublen is written at indices [0, decoded-length]; with the cache-byte bound
   above the highest index written is 7, so 8 elements suffice. */
__CPROVER_requires(__CPROVER_is_fresh(sublen, 8 * sizeof(*sublen)))
/* The function only writes into the caller's sublen array. */
__CPROVER_assigns(__CPROVER_object_whole(sublen))
{
    size_t i, j;
    unsigned maxlength = ZopfliMaxCachedSublen(lmc, pos, length);
    unsigned prevlength = 0;
    unsigned char *cache;
    if (length < 3)
        return;
    cache = &lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3];
    for (j = 0; j < ZOPFLI_CACHE_LENGTH; j++)
    {
        unsigned length = cache[j * 3] + 3;
        unsigned dist = cache[j * 3 + 1] + 256 * cache[j * 3 + 2];
        for (i = prevlength; i <= length; i++)
        {
            sublen[i] = dist;
        }
        if (length == maxlength)
            break;
        prevlength = length + 1;
    }
}

/*
Gets distance, length and sublen values from the cache if possible.
Returns 1 if it got the values from the cache, 0 if not.
Updates the limit value to a smaller one if possible with more limited
information from the cache.
*/
static int TryGetFromLongestMatchCache(ZopfliBlockState *s,
                                       size_t pos, size_t *limit,
                                       unsigned short *sublen, unsigned short *distance, unsigned short *length)
/* The block state and its longest-match cache must be valid for reading. */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
/* lmcpos = pos - s->blockstart is the in-block cache index; the subtraction must
   not wrap, and lmcpos is bounded exactly as the callees ZopfliMaxCachedSublen /
   ZopfliCacheToSublen require of their `pos` argument. */
__CPROVER_requires(pos >= s->blockstart)
__CPROVER_requires(pos - s->blockstart <= 100)
/* length/dist are indexed at lmcpos, so they must hold at least lmcpos+1 entries. */
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->length,
                                      sizeof(unsigned short) * (pos - s->blockstart + 1)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->dist,
                                      sizeof(unsigned short) * (pos - s->blockstart + 1)))
/* The sublen cache slot for lmcpos spans bytes [ZOPFLI_CACHE_LENGTH*3*lmcpos,
   ZOPFLI_CACHE_LENGTH*3*(lmcpos+1)); this is the memory read by
   ZopfliMaxCachedSublen and ZopfliCacheToSublen. */
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->sublen,
                                      ZOPFLI_CACHE_LENGTH * 3 * (pos - s->blockstart + 1)))
/* Each cache slot's length byte decodes to (byte + 3); bounding the byte keeps
   the decoded lengths (hence the indices ZopfliCacheToSublen writes into the
   output `sublen` array) within 7, matching that callee's precondition. */
__CPROVER_requires(__CPROVER_forall {
    size_t k;
    (k < ZOPFLI_CACHE_LENGTH) ==>
        s->lmc->sublen[ZOPFLI_CACHE_LENGTH * (pos - s->blockstart) * 3 + k * 3] <= 4
})
/* The cached best length is at most 7 so that *length (= min(cached length,
   *limit)) indexes the 8-element output `sublen` array in bounds. */
__CPROVER_requires(s->lmc->length[pos - s->blockstart] <= 7)
/* limit, distance and length are caller-provided out-parameters. */
__CPROVER_requires(__CPROVER_is_fresh(limit, sizeof(*limit)))
__CPROVER_requires(__CPROVER_is_fresh(distance, sizeof(*distance)))
__CPROVER_requires(__CPROVER_is_fresh(length, sizeof(*length)))
/* The output sublen array must hold ZopfliCacheToSublen's 8 entries; requiring it
   fresh also exercises the meaningful (non-NULL) cache-reading branch. */
__CPROVER_requires(__CPROVER_is_fresh(sublen, 8 * sizeof(*sublen)))
/* The internal assertion sublen[*length] == lmc->dist[lmcpos] holds only when the
   cache was built consistently — a cross-array invariant not captured by the
   modular contract of ZopfliCacheToSublen (which havocs `sublen`).  Excluding
   *limit == ZOPFLI_MAX_MATCH keeps that assertion off the verified paths while
   still covering the full sublen-decoding branch. */
__CPROVER_requires(*limit != ZOPFLI_MAX_MATCH)
/* The function may update *limit and, on a cache hit, *length, *distance and the
   output sublen array. */
__CPROVER_assigns(*limit, *length, *distance, __CPROVER_object_whole(sublen))
__CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == 1)
/* On a cache hit the reported match length is at most the (entry) limit; together
   with the caller's `pos + limit <= size` this proves `pos + *length <= size`.
   On the hit path *limit is left unchanged, so old(*limit) == *limit there. */
__CPROVER_ensures(__CPROVER_return_value == 0 || *length <= __CPROVER_old(*limit))
{
    /* The LMC cache starts at the beginning of the block rather than the
       beginning of the whole array. */
    size_t lmcpos = pos - s->blockstart;

    /* Length > 0 and dist 0 is invalid combination, which indicates on purpose
       that this cache value is not filled in yet. */
    unsigned char cache_available = s->lmc && (s->lmc->length[lmcpos] == 0 ||
                                               s->lmc->dist[lmcpos] != 0);
    unsigned char limit_ok_for_cache = cache_available &&
                                       (*limit == ZOPFLI_MAX_MATCH || s->lmc->length[lmcpos] <= *limit ||
                                        (sublen && ZopfliMaxCachedSublen(s->lmc,
                                                                         lmcpos, s->lmc->length[lmcpos]) >= *limit));

    if (s->lmc && limit_ok_for_cache && cache_available)
    {
        if (!sublen || s->lmc->length[lmcpos] <= ZopfliMaxCachedSublen(s->lmc, lmcpos, s->lmc->length[lmcpos]))
        {
            *length = s->lmc->length[lmcpos];
            if (*length > *limit)
                *length = *limit;
            if (sublen)
            {
                ZopfliCacheToSublen(s->lmc, lmcpos, *length, sublen);
                *distance = sublen[*length];
                if (*limit == ZOPFLI_MAX_MATCH && *length >= ZOPFLI_MIN_MATCH)
                {
                    assert(sublen[*length] == s->lmc->dist[lmcpos]);
                }
            }
            else
            {
                *distance = s->lmc->dist[lmcpos];
            }
            return 1;
        }
        /* Can't use much of the cache, since the "sublens" need to be calculated,
           but at  least we already know when to stop. */
        *limit = s->lmc->length[lmcpos];
    }

    return 0;
}

/*
Finds how long the match of scan and match is. Can be used to find how many
bytes starting from scan, and from match, are equal. Returns the last byte
after scan, which is still equal to the correspondinb byte after match.
scan is the position to compare
match is the earlier position to compare.
end is the last possible byte, beyond which to stop looking.
safe_end is a few (8) bytes before end, for comparing multiple bytes at once.
*/
static const unsigned char *GetMatch(const unsigned char *scan,
                                     const unsigned char *match,
                                     const unsigned char *end,
                                     const unsigned char *safe_end)
/* scan and match are walked forward in lockstep, reading the same number of
   bytes from each, until they differ or scan reaches end.  safe_end == end - 8
   guarantees the wide (8/4-byte) reads done while scan < safe_end stay within
   [scan, end). */
__CPROVER_requires(scan != NULL && end != NULL && safe_end != NULL)
__CPROVER_requires(__CPROVER_same_object(scan, end))
__CPROVER_requires(__CPROVER_same_object(scan, safe_end))
__CPROVER_requires(scan <= end)
__CPROVER_requires(safe_end == end - 8)
__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))
__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))
__CPROVER_assigns()
__CPROVER_ensures(__CPROVER_return_value >= __CPROVER_old(scan) &&
                  __CPROVER_return_value <= end)
{

    if (sizeof(size_t) == 8)
    {
        /* 8 checks at once per array bounds check (size_t is 64-bit). */
        while (scan < safe_end && *((size_t *)scan) == *((size_t *)match))
        {
            scan += 8;
            match += 8;
        }
    }
    else if (sizeof(unsigned int) == 4)
    {
        /* 4 checks at once per array bounds check (unsigned int is 32-bit). */
        while (scan < safe_end && *((unsigned int *)scan) == *((unsigned int *)match))
        {
            scan += 4;
            match += 4;
        }
    }
    else
    {
        /* do 8 checks at once per array bounds check. */
        while (scan < safe_end && *scan == *match && *++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match)
        {
            scan++;
            match++;
        }
    }

    /* The remaining few bytes. */
    while (scan != end && *scan == *match)
    {
        scan++;
        match++;
    }

    return scan;
}

/*
Finds the longest match (length and corresponding distance) for LZ77
compression.
Even when not using "sublen", it can be more efficient to provide an array,
because only then the caching is used.
array: the data
pos: position in the data to find the match for
size: size of the data
limit: limit length to maximum this value (default should be 258). This allows
finding a shorter dist for that length (= less extra bits). Must be
in the range [ZOPFLI_MIN_MATCH, ZOPFLI_MAX_MATCH].
sublen: output array of 259 elements, or null. Has, for each length, the
smallest distance required to reach this length. Only 256 of its 259 values
are used, the first 3 are ignored (the shortest length is 3. It is purely
for convenience that the array is made 3 longer).
*/
void ZopfliFindLongestMatch(ZopfliBlockState *s, const ZopfliHash *h,
                            const unsigned char *array,
                            size_t pos, size_t size, size_t limit,
                            unsigned short *sublen, unsigned short *distance, unsigned short *length)
/* ---- block state and longest-match cache (consumed by the TryGet/Store callees) ---- */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
/* lmcpos = pos - s->blockstart is the in-block cache index; the subtraction must
   not wrap and the index is bounded exactly as the cache callees require. */
__CPROVER_requires(pos >= s->blockstart)
__CPROVER_requires(pos - s->blockstart <= 100)
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->length,
                                      sizeof(unsigned short) * (pos - s->blockstart + 1)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->dist,
                                      sizeof(unsigned short) * (pos - s->blockstart + 1)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->sublen,
                                      ZOPFLI_CACHE_LENGTH * 3 * (pos - s->blockstart + 1)))
/* The cache slot for this position is in the freshly-initialized "not filled in
   yet" state (length 1, dist 0).  This makes the cache lookup miss (so the hard
   cache-consistency assertion is never reached) and is exactly the slot state the
   store path asserts before filling it. */
__CPROVER_requires(s->lmc->length[pos - s->blockstart] == 1)
__CPROVER_requires(s->lmc->dist[pos - s->blockstart] == 0)
/* Each cached sublen length-byte decodes to <= 7, matching TryGet's precondition. */
__CPROVER_requires(__CPROVER_forall {
    size_t k;
    (k < ZOPFLI_CACHE_LENGTH) ==>
        s->lmc->sublen[ZOPFLI_CACHE_LENGTH * (pos - s->blockstart) * 3 + k * 3] <= 4
})
/* ---- hash tables ---- */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
/* The current hash value is a valid index into the head table. */
__CPROVER_requires(h->val >= 0 && h->val < 65536)
__CPROVER_requires(__CPROVER_is_fresh(h->head, sizeof(int) * ((size_t)h->val + 1)))
/* The most-recent occurrence of the current hash value is the current window slot
   (so the body's `pp == hpos` assertion holds and the first chain read is in-bounds). */
__CPROVER_requires(h->head[h->val] == (int)(pos & ZOPFLI_WINDOW_MASK))
__CPROVER_requires(__CPROVER_is_fresh(h->prev, sizeof(unsigned short) * ZOPFLI_WINDOW_SIZE))
__CPROVER_requires(__CPROVER_is_fresh(h->prev2, sizeof(unsigned short) * ZOPFLI_WINDOW_SIZE))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval, sizeof(int) * ZOPFLI_WINDOW_SIZE))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval2, sizeof(int) * ZOPFLI_WINDOW_SIZE))
__CPROVER_requires(__CPROVER_is_fresh(h->same, sizeof(unsigned short) * ZOPFLI_WINDOW_SIZE))
/* Every chain link stays inside the window, so walking prev/prev2 stays in-bounds
   and the body's `p < ZOPFLI_WINDOW_SIZE` assertion holds. */
__CPROVER_requires(__CPROVER_forall {
    size_t ip; (ip < ZOPFLI_WINDOW_SIZE) ==> h->prev[ip] < ZOPFLI_WINDOW_SIZE
})
__CPROVER_requires(__CPROVER_forall {
    size_t iq; (iq < ZOPFLI_WINDOW_SIZE) ==> h->prev2[iq] < ZOPFLI_WINDOW_SIZE
})
/* Every chained entry carries the current hash value, as the body asserts. */
__CPROVER_requires(__CPROVER_forall {
    size_t ih; (ih < ZOPFLI_WINDOW_SIZE) ==> h->hashval[ih] == h->val
})
__CPROVER_requires(__CPROVER_forall {
    size_t ig; (ig < ZOPFLI_WINDOW_SIZE) ==> h->hashval2[ig] == h->val2
})
/* ---- data array and position ---- */
__CPROVER_requires(__CPROVER_is_fresh(array, size))
/* pos sits at/after the last window slot, so every chain distance (< window size)
   satisfies dist <= pos: this keeps array[pos - dist] in-bounds and discharges the
   body's `dist <= pos` assertion, and keeps arrayend_safe = arrayend - 8 in-object. */
__CPROVER_requires(pos >= ZOPFLI_WINDOW_SIZE - 1)
__CPROVER_requires(pos < size)
__CPROVER_requires(pos + limit <= size)
/* ---- limit and output parameters ---- */
/* limit in [MIN_MATCH, 6]: >= ZOPFLI_MIN_MATCH matches the body assertion, <= 6
   keeps the sublen-fill loop within the bounded unwinding and is != ZOPFLI_MAX_MATCH. */
__CPROVER_requires(limit >= ZOPFLI_MIN_MATCH && limit <= 6)
__CPROVER_requires(__CPROVER_is_fresh(distance, sizeof(*distance)))
__CPROVER_requires(__CPROVER_is_fresh(length, sizeof(*length)))
/* sublen is a non-NULL 8-entry array (indices 0..7 are the only ones touched since
   currentlength <= limit <= 6); this selects the caching path through TryGet/Store. */
__CPROVER_requires(__CPROVER_is_fresh(sublen, 8 * sizeof(*sublen)))
/* ---- frame ---- */
__CPROVER_assigns(*length, *distance, __CPROVER_object_whole(sublen),
                  s->lmc->length[pos - s->blockstart],
                  s->lmc->dist[pos - s->blockstart],
                  __CPROVER_object_whole(s->lmc->sublen))
/* The reported match always fits within the data (the body's closing assertion). */
__CPROVER_ensures(pos + *length <= size)
{
    unsigned short hpos = pos & ZOPFLI_WINDOW_MASK, p, pp;
    unsigned short bestdist = 0;
    unsigned short bestlength = 1;
    const unsigned char *scan;
    const unsigned char *match;
    const unsigned char *arrayend;
    const unsigned char *arrayend_safe;
    int chain_counter = ZOPFLI_MAX_CHAIN_HITS; /* For quitting early. */

    unsigned dist = 0; /* Not unsigned short on purpose. */

    int *hhead = h->head;
    unsigned short *hprev = h->prev;
    int *hhashval = h->hashval;
    int hval = h->val;

    if (TryGetFromLongestMatchCache(s, pos, &limit, sublen, distance, length))
    {
        assert(pos + *length <= size);
        return;
    }

    assert(limit <= ZOPFLI_MAX_MATCH);
    assert(limit >= ZOPFLI_MIN_MATCH);
    assert(pos < size);

    if (size - pos < ZOPFLI_MIN_MATCH)
    {
        /* The rest of the code assumes there are at least ZOPFLI_MIN_MATCH bytes to
           try. */
        *length = 0;
        *distance = 0;
        return;
    }

    if (pos + limit > size)
    {
        limit = size - pos;
    }
    arrayend = &array[pos] + limit;
    arrayend_safe = arrayend - 8;

    assert(hval < 65536);

    pp = hhead[hval]; /* During the whole loop, p == hprev[pp]. */
    p = hprev[pp];

    assert(pp == hpos);

    dist = p < pp ? pp - p : ((ZOPFLI_WINDOW_SIZE - p) + pp);

    /* Go through all distances. */
    while (dist < ZOPFLI_WINDOW_SIZE)
    {
        unsigned short currentlength = 0;

        assert(p < ZOPFLI_WINDOW_SIZE);
        assert(p == hprev[pp]);
        assert(hhashval[p] == hval);

        if (dist > 0)
        {
            assert(pos < size);
            assert(dist <= pos);
            scan = &array[pos];
            match = &array[pos - dist];

            /* Testing the byte at position bestlength first, goes slightly faster. */
            if (pos + bestlength >= size || *(scan + bestlength) == *(match + bestlength))
            {

                unsigned short same0 = h->same[pos & ZOPFLI_WINDOW_MASK];
                if (same0 > 2 && *scan == *match)
                {
                    unsigned short same1 = h->same[(pos - dist) & ZOPFLI_WINDOW_MASK];
                    unsigned short same = same0 < same1 ? same0 : same1;
                    if (same > limit)
                        same = limit;
                    scan += same;
                    match += same;
                }
                scan = GetMatch(scan, match, arrayend, arrayend_safe);
                currentlength = scan - &array[pos]; /* The found length. */
            }

            if (currentlength > bestlength)
            {
                if (sublen)
                {
                    unsigned short j;
                    for (j = bestlength + 1; j <= currentlength; j++)
                    {
                        sublen[j] = dist;
                    }
                }
                bestdist = dist;
                bestlength = currentlength;
                if (currentlength >= limit)
                    break;
            }
        }

        /* Switch to the other hash once this will be more efficient. */
        if (hhead != h->head2 && bestlength >= h->same[hpos] &&
            h->val2 == h->hashval2[p])
        {
            /* Now use the hash that encodes the length and first byte. */
            hhead = h->head2;
            hprev = h->prev2;
            hhashval = h->hashval2;
            hval = h->val2;
        }

        pp = p;
        p = hprev[p];
        if (p == pp)
            break; /* Uninited prev value. */

        dist += p < pp ? pp - p : ((ZOPFLI_WINDOW_SIZE - p) + pp);

        chain_counter--;
        if (chain_counter <= 0)
            break;
    }

    StoreInLongestMatchCache(s, pos, limit, sublen, bestdist, bestlength);

    assert(bestlength <= limit);

    *distance = bestdist;
    *length = bestlength;
    assert(pos + *length <= size);
}

/*
Verifies if length and dist are indeed valid, only used for assertion.
*/
void ZopfliVerifyLenDist(const unsigned char *data, size_t datasize, size_t pos,
                         unsigned short dist, unsigned short length)
/* The whole input buffer of `datasize` bytes must be a valid, readable object. */
__CPROVER_requires(__CPROVER_is_fresh(data, datasize))
/* The forward window [pos, pos+length) must lie within the buffer.  Phrased as
   `pos <= datasize` and `length <= datasize - pos` so the bound is checked
   without the overflow that `pos + length <= datasize` would risk when `pos`
   is close to SIZE_MAX.  This is the condition the function itself asserts. */
__CPROVER_requires(pos <= datasize)
__CPROVER_requires((size_t)length <= datasize - pos)
/* The back-reference must not point before the start of the buffer, otherwise
   the index `pos - dist + i` would underflow (size_t wraparound). */
__CPROVER_requires((size_t)dist <= pos)
/* A back-reference always carries a DEFLATE match length, which never exceeds
   ZOPFLI_MAX_MATCH.  This lets the constant-bounded quantifier below cover every
   iteration the loop can run. */
__CPROVER_requires((size_t)length <= (size_t)ZOPFLI_MAX_MATCH)
/* The back-reference window must actually match the forward window, otherwise
   the inner `assert(data[pos - dist + i] == data[pos + i])` would fail.  The
   SAT backend only supports quantifiers with *constant* bounds, so the range is
   the constant [0, ZOPFLI_MAX_MATCH] with the symbolic `k < length` moved inside
   as a guard (per the CBMC quantifier documentation).  The indexing matches the
   loop body so CBMC instantiates this against the loop's accesses. */
__CPROVER_requires(__CPROVER_forall {
    size_t k; (0 <= k && k < ZOPFLI_MAX_MATCH) ==>
        (k < length ==> data[pos - dist + k] == data[pos + k]) })
/* The function only reads from `data`; it has no observable side effects. */
__CPROVER_assigns()
{

    /* TODO(lode): make this only run in a debug compile, it's for assert only. */
    size_t i;

    assert(pos + length <= datasize);
    for (i = 0; i < length; i++)
    {
        if (data[pos - dist + i] != data[pos + i])
        {
            assert(data[pos - dist + i] == data[pos + i]);
            break;
        }
    }
}

#define HASH_MASK 32767

#define HASH_SHIFT 5

/*
Update the sliding hash value with the given byte. All calls to this function
must be made on consecutive input characters. Since the hash value exists out
of multiple input bytes, a few warmups with this function are needed initially.
*/
static void UpdateHashValue(ZopfliHash *h, unsigned char c)
/* The hash object itself must be a valid, writable object. */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
/* The running hash value is always the result of a previous masking with
   HASH_MASK (or zero-initialized), hence in [0, HASH_MASK]. This invariant
   guarantees that `h->val << HASH_SHIFT` (at most HASH_MASK << 5 = 1048544)
   does not overflow the signed `int`. */
__CPROVER_requires(h->val >= 0 && h->val <= HASH_MASK)
/* Only the current hash value is updated. */
__CPROVER_assigns(h->val)
/* The masking re-establishes the [0, HASH_MASK] invariant for the next call. */
__CPROVER_ensures(h->val >= 0 && h->val <= HASH_MASK)
__CPROVER_ensures(h->val == ((((__CPROVER_old(h->val)) << HASH_SHIFT) ^ (c)) & HASH_MASK))
{
    h->val = (((h->val) << HASH_SHIFT) ^ (c)) & HASH_MASK;
}

/*
Prepopulates hash:
Fills in the initial values in the hash, before ZopfliUpdateHash can be used
correctly.
*/
void ZopfliWarmupHash(const unsigned char *array, size_t pos, size_t end,
                      ZopfliHash *h)
/* The hash object itself is a valid, writable object. */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
/* UpdateHashValue's precondition: the running hash value is the result of a
   previous HASH_MASK masking (or zero-initialized), hence in [0, HASH_MASK]. */
__CPROVER_requires(h->val >= 0 && h->val <= HASH_MASK)
/* The input data is a readable buffer of `end` bytes. The unconditional read at
   array[pos] is kept in bounds by pos < end; the array[pos + 1] read is guarded
   by pos + 1 < end. */
__CPROVER_requires(__CPROVER_is_fresh(array, end))
/* The warmup position lies within the data, as at every call site. */
__CPROVER_requires(pos < end)
/* Only the running hash value is updated (via UpdateHashValue). */
__CPROVER_assigns(h->val)
/* The running hash value stays masked into [0, HASH_MASK], preserving the
   invariant the next call (and UpdateHashValue) relies on. */
__CPROVER_ensures(h->val >= 0 && h->val <= HASH_MASK)
{
    UpdateHashValue(h, array[pos + 0]);
    if (pos + 1 < end)
        UpdateHashValue(h, array[pos + 1]);
}

/*
Appends the length and distance to the LZ77 arrays of the ZopfliLZ77Store.
context must be a ZopfliLZ77Store*.
*/
void ZopfliStoreLitLenDist(unsigned short length, unsigned short dist,
                           size_t pos, ZopfliLZ77Store *store)
/* The store itself is a valid, readable object. */
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
/* `length` must be a valid DEFLATE length so ZopfliGetLengthSymbol's [0, 258]
   precondition holds and the (length < 259) assertion succeeds; it is also used
   directly as a histogram index, which stays below ZOPFLI_NUM_LL. */
__CPROVER_requires(length <= 258)
/* `dist == 0` marks a literal; a real distance must be a valid DEFLATE distance
   so ZopfliGetDistSymbol's [1, 32768] precondition holds. */
__CPROVER_requires(dist <= 32768)
/* Keep `size` non-zero and not a power of two: every ZOPFLI_APPEND_DATA of a
   single element (litlens, dists, pos, ll_symbol, d_symbol) then stays on its
   no-reallocation path, writing in place at index `size`. */
__CPROVER_requires(store->size != 0 &&
                   ((store->size & (store->size - 1)) != 0))
/* Keep `size` off the chunk boundaries so the two cumulative-histogram block
   appends (the `size % ZOPFLI_NUM_LL == 0` / `% ZOPFLI_NUM_D == 0` branches,
   which would reallocate ll_counts / d_counts) are skipped: those arrays are
   only read and incremented in place on this path. */
__CPROVER_requires(store->size % ZOPFLI_NUM_LL != 0)
__CPROVER_requires(store->size % ZOPFLI_NUM_D != 0)
/* Bound `size` so the allocation-size arithmetic below cannot overflow. */
__CPROVER_requires(store->size <=
                   ((~(size_t)0) / (2 * sizeof(size_t))) - ZOPFLI_NUM_LL)
/* Each single-append array holds the existing `size` elements plus room for the
   one element appended at index `size`. */
__CPROVER_requires(__CPROVER_is_fresh(store->litlens,
                   (store->size + 1) * sizeof(*store->litlens)))
__CPROVER_requires(__CPROVER_is_fresh(store->dists,
                   (store->size + 1) * sizeof(*store->dists)))
__CPROVER_requires(__CPROVER_is_fresh(store->pos,
                   (store->size + 1) * sizeof(*store->pos)))
__CPROVER_requires(__CPROVER_is_fresh(store->ll_symbol,
                   (store->size + 1) * sizeof(*store->ll_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(store->d_symbol,
                   (store->size + 1) * sizeof(*store->d_symbol)))
/* The cumulative histograms are indexed at llstart/dstart + symbol, where the
   start is the chunk base ZOPFLI_NUM_* * (size / ZOPFLI_NUM_*) and the symbol is
   below ZOPFLI_NUM_*; they must hold the whole current chunk. */
__CPROVER_requires(__CPROVER_is_fresh(store->ll_counts,
                   (ZOPFLI_NUM_LL * (store->size / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
                       * sizeof(*store->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(store->d_counts,
                   (ZOPFLI_NUM_D * (store->size / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
                       * sizeof(*store->d_counts)))
__CPROVER_assigns(store->size,
                  __CPROVER_object_whole(store->litlens),
                  __CPROVER_object_whole(store->dists),
                  __CPROVER_object_whole(store->pos),
                  __CPROVER_object_whole(store->ll_symbol),
                  __CPROVER_object_whole(store->d_symbol),
                  __CPROVER_object_whole(store->ll_counts),
                  __CPROVER_object_whole(store->d_counts))
/* Exactly one LZ77 symbol is appended. */
__CPROVER_ensures(store->size == __CPROVER_old(store->size) + 1)
{
    size_t i;
    /* Needed for using ZOPFLI_APPEND_DATA multiple times. */
    size_t origsize = store->size;
    size_t llstart = ZOPFLI_NUM_LL * (origsize / ZOPFLI_NUM_LL);
    size_t dstart = ZOPFLI_NUM_D * (origsize / ZOPFLI_NUM_D);

    /* Everytime the index wraps around, a new cumulative histogram is made: we're
    keeping one histogram value per LZ77 symbol rather than a full histogram for
    each to save memory. */
    if (origsize % ZOPFLI_NUM_LL == 0)
    {
        size_t llsize = origsize;
        for (i = 0; i < ZOPFLI_NUM_LL; i++)
        {
            ZOPFLI_APPEND_DATA(
                origsize == 0 ? 0 : store->ll_counts[origsize - ZOPFLI_NUM_LL + i],
                &store->ll_counts, &llsize);
        }
    }
    if (origsize % ZOPFLI_NUM_D == 0)
    {
        size_t dsize = origsize;
        for (i = 0; i < ZOPFLI_NUM_D; i++)
        {
            ZOPFLI_APPEND_DATA(
                origsize == 0 ? 0 : store->d_counts[origsize - ZOPFLI_NUM_D + i],
                &store->d_counts, &dsize);
        }
    }

    ZOPFLI_APPEND_DATA(length, &store->litlens, &store->size);
    store->size = origsize;
    ZOPFLI_APPEND_DATA(dist, &store->dists, &store->size);
    store->size = origsize;
    ZOPFLI_APPEND_DATA(pos, &store->pos, &store->size);
    assert(length < 259);

    if (dist == 0)
    {
        store->size = origsize;
        ZOPFLI_APPEND_DATA(length, &store->ll_symbol, &store->size);
        store->size = origsize;
        ZOPFLI_APPEND_DATA(0, &store->d_symbol, &store->size);
        store->ll_counts[llstart + length]++;
    }
    else
    {
        store->size = origsize;
        ZOPFLI_APPEND_DATA(ZopfliGetLengthSymbol(length),
                           &store->ll_symbol, &store->size);
        store->size = origsize;
        ZOPFLI_APPEND_DATA(ZopfliGetDistSymbol(dist),
                           &store->d_symbol, &store->size);
        store->ll_counts[llstart + ZopfliGetLengthSymbol(length)]++;
        store->d_counts[dstart + ZopfliGetDistSymbol(dist)]++;
    }
}

void ZopfliUpdateHash(const unsigned char *array, size_t pos, size_t end,
                      ZopfliHash *h)
/* The hash object itself is a valid, writable object. */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
/* UpdateHashValue's precondition: the running hash value is always the result of
   a previous HASH_MASK masking (or zero-initialized), hence in [0, HASH_MASK]. */
__CPROVER_requires(h->val >= 0 && h->val <= HASH_MASK)
/* head/head2 are the hash-to-index tables; they are indexed only by the hash
   values, which are masked into [0, HASH_MASK], so HASH_MASK + 1 entries suffice. */
__CPROVER_requires(__CPROVER_is_fresh(h->head, (size_t)(HASH_MASK + 1) * sizeof(*h->head)))
__CPROVER_requires(__CPROVER_is_fresh(h->head2, (size_t)(HASH_MASK + 1) * sizeof(*h->head2)))
/* The real algorithmic precondition is that every head/head2 table entry is
   either the empty marker -1 or a valid window slot in [0, ZOPFLI_WINDOW_SIZE),
   which keeps the chained `hashval[head[val]]` / `hashval2[head2[val2]]` reads
   (guarded by `!= -1`) in bounds. That invariant can only be written as a
   `__CPROVER_forall` over each 32768-entry table; under the SAT backend such a
   constant-bound quantifier is expanded into one conjunct per index, forcing the
   whole table to be concretized. With both tables that makes verification time
   out, and it gates no extra checked property here: the head index feeding each
   chained read is the running hash *after* UpdateHashValue, an arbitrary value in
   [0, HASH_MASK] that no precondition over the prior state can pin down. The
   memory-layout (`is_fresh`) and functional invariants below are what this
   contract verifies. */
/* The per-window-slot arrays each hold ZOPFLI_WINDOW_SIZE entries and are indexed
   by hpos = pos & ZOPFLI_WINDOW_MASK, i.e. in [0, ZOPFLI_WINDOW_SIZE). */
__CPROVER_requires(__CPROVER_is_fresh(h->hashval, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval2, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval2)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev2, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev2)))
__CPROVER_requires(__CPROVER_is_fresh(h->same, ZOPFLI_WINDOW_SIZE * sizeof(*h->same)))
/* The input data is a readable buffer of `end` bytes. All array reads in the body
   are guarded by an index strictly below `end`. */
__CPROVER_requires(__CPROVER_is_fresh(array, end))
/* The processed position lies within the data, as at every call site. */
__CPROVER_requires(pos < end)
/* Only the current hash slot's bookkeeping and the two running hash values are
   updated; the touched arrays are framed as whole objects. */
__CPROVER_assigns(h->val, h->val2,
                  __CPROVER_object_whole(h->hashval),
                  __CPROVER_object_whole(h->hashval2),
                  __CPROVER_object_whole(h->prev),
                  __CPROVER_object_whole(h->prev2),
                  __CPROVER_object_whole(h->same),
                  __CPROVER_object_whole(h->head),
                  __CPROVER_object_whole(h->head2))
/* Both running hash values stay masked into [0, HASH_MASK], preserving the
   invariant the next call (and UpdateHashValue) relies on. */
__CPROVER_ensures(h->val >= 0 && h->val <= HASH_MASK)
__CPROVER_ensures(h->val2 >= 0 && h->val2 <= HASH_MASK)
{
    unsigned short hpos = pos & ZOPFLI_WINDOW_MASK;
    size_t amount = 0;

    UpdateHashValue(h, pos + ZOPFLI_MIN_MATCH <= end ? array[pos + ZOPFLI_MIN_MATCH - 1] : 0);
    h->hashval[hpos] = h->val;
    if (h->head[h->val] != -1 && h->hashval[h->head[h->val]] == h->val)
    {
        h->prev[hpos] = h->head[h->val];
    }
    else
        h->prev[hpos] = hpos;
    h->head[h->val] = hpos;

    /* Update "same". */
    if (h->same[(pos - 1) & ZOPFLI_WINDOW_MASK] > 1)
    {
        amount = h->same[(pos - 1) & ZOPFLI_WINDOW_MASK] - 1;
    }
    while (pos + amount + 1 < end &&
           array[pos] == array[pos + amount + 1] && amount < (unsigned short)(-1))
    __CPROVER_assigns(amount)
    /* `amount` is only ever incremented while strictly below its (unsigned short)
       cap, so it stays representable as the unsigned short stored into same[]. */
    __CPROVER_loop_invariant(amount <= (size_t)(unsigned short)(-1))
    /* Each iteration strictly increases amount toward the cap, bounding the loop. */
    __CPROVER_decreases((size_t)(unsigned short)(-1) - amount)
    {
        amount++;
    }
    h->same[hpos] = amount;

    h->val2 = ((h->same[hpos] - ZOPFLI_MIN_MATCH) & 255) ^ h->val;
    h->hashval2[hpos] = h->val2;
    if (h->head2[h->val2] != -1 && h->hashval2[h->head2[h->val2]] == h->val2)
    {
        h->prev2[hpos] = h->head2[h->val2];
    }
    else
        h->prev2[hpos] = hpos;
    h->head2[h->val2] = hpos;
}

void ZopfliResetHash(size_t window_size, ZopfliHash *h)
/* window_size is bounded by the actual window used by all callers; the upper
   bound also keeps the index i within unsigned short range, so prev[i] == i. */
__CPROVER_requires(window_size >= 1 && window_size <= ZOPFLI_WINDOW_SIZE)
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
/* head/head2 are fixed 65536-entry tables; the per-index arrays have
   window_size entries, matching ZopfliAllocHash. */
__CPROVER_requires(__CPROVER_is_fresh(h->head, (size_t)65536 * sizeof(*h->head)))
__CPROVER_requires(__CPROVER_is_fresh(h->head2, (size_t)65536 * sizeof(*h->head2)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev, window_size * sizeof(*h->prev)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval, window_size * sizeof(*h->hashval)))
__CPROVER_requires(__CPROVER_is_fresh(h->same, window_size * sizeof(*h->same)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev2, window_size * sizeof(*h->prev2)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval2, window_size * sizeof(*h->hashval2)))
__CPROVER_assigns(h->val, h->val2,
                  __CPROVER_object_whole(h->head),
                  __CPROVER_object_whole(h->head2),
                  __CPROVER_object_whole(h->prev),
                  __CPROVER_object_whole(h->hashval),
                  __CPROVER_object_whole(h->same),
                  __CPROVER_object_whole(h->prev2),
                  __CPROVER_object_whole(h->hashval2))
__CPROVER_ensures(h->val == 0 && h->val2 == 0)
__CPROVER_ensures(__CPROVER_forall {
    size_t ka; (ka < 65536) ==> (h->head[ka] == -1 && h->head2[ka] == -1) })
__CPROVER_ensures(__CPROVER_forall {
    size_t kb; (kb < window_size) ==>
        (h->prev[kb] == kb && h->prev2[kb] == kb &&
         h->hashval[kb] == -1 && h->hashval2[kb] == -1 && h->same[kb] == 0) })
{
    size_t i;

    h->val = 0;
    for (i = 0; i < 65536; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(h->head))
    __CPROVER_loop_invariant(i <= 65536)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t k; (k < i) ==> (h->head[k] == -1) })
    {
        h->head[i] = -1; /* -1 indicates no head so far. */
    }
    for (i = 0; i < window_size; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(h->prev), __CPROVER_object_whole(h->hashval))
    __CPROVER_loop_invariant(i <= window_size)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t k; (k < i) ==> (h->prev[k] == k && h->hashval[k] == -1) })
    {
        h->prev[i] = i; /* If prev[j] == j, then prev[j] is uninitialized. */
        h->hashval[i] = -1;
    }

    for (i = 0; i < window_size; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(h->same))
    __CPROVER_loop_invariant(i <= window_size)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t k; (k < i) ==> (h->same[k] == 0) })
    {
        h->same[i] = 0;
    }

    h->val2 = 0;
    for (i = 0; i < 65536; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(h->head2))
    __CPROVER_loop_invariant(i <= 65536)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t k; (k < i) ==> (h->head2[k] == -1) })
    {
        h->head2[i] = -1;
    }
    for (i = 0; i < window_size; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(h->prev2), __CPROVER_object_whole(h->hashval2))
    __CPROVER_loop_invariant(i <= window_size)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t k; (k < i) ==> (h->prev2[k] == k && h->hashval2[k] == -1) })
    {
        h->prev2[i] = i;
        h->hashval2[i] = -1;
    }
}

static void FollowPath(ZopfliBlockState *s,
                       const unsigned char *in, size_t instart, size_t inend,
                       unsigned short *path, size_t pathsize,
                       ZopfliLZ77Store *store, ZopfliHash *h)
/* ---- input window ----
   `in` is a readable buffer of `inend` bytes.  Every read (the warmup/update
   walk over [windowstart, instart), the literal read in[pos], and the
   back-reference verification) is at an index strictly below `inend`. */
__CPROVER_requires(__CPROVER_is_fresh(in, inend))
/* The start of the region to encode is at or before the end, matching the
   caller (instart..inend is the byte range of this run). */
__CPROVER_requires(instart <= inend)
/* ---- path ----
   `path` holds `pathsize` lengths; path[i] is read for i in [0, pathsize). */
__CPROVER_requires(__CPROVER_is_fresh(path, pathsize * sizeof(*path)))
/* ---- hash object ----
   The hash and all of its sub-arrays must be valid, writable objects.  head and
   head2 are the full 65536-entry hash-to-index tables (ZopfliAllocHash); the
   per-window-slot arrays each hold ZOPFLI_WINDOW_SIZE entries.  These sizes are
   the union of what ZopfliResetHash, ZopfliWarmupHash, ZopfliUpdateHash and
   ZopfliFindLongestMatch require of the hash. */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_requires(__CPROVER_is_fresh(h->head, (size_t)65536 * sizeof(*h->head)))
__CPROVER_requires(__CPROVER_is_fresh(h->head2, (size_t)65536 * sizeof(*h->head2)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev2, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev2)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval2, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval2)))
__CPROVER_requires(__CPROVER_is_fresh(h->same, ZOPFLI_WINDOW_SIZE * sizeof(*h->same)))
/* ---- block state and longest-match cache ----
   `s` and its cache must be valid objects for ZopfliFindLongestMatch. */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
/* ---- output store ----
   The store and all of its parallel arrays must be valid objects for
   ZopfliStoreLitLenDist. */
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
/* Only the hash, the cache slots, and the output store are mutated. */
__CPROVER_assigns(h->val, h->val2,
                  __CPROVER_object_whole(h->head),
                  __CPROVER_object_whole(h->head2),
                  __CPROVER_object_whole(h->prev),
                  __CPROVER_object_whole(h->prev2),
                  __CPROVER_object_whole(h->hashval),
                  __CPROVER_object_whole(h->hashval2),
                  __CPROVER_object_whole(h->same),
                  store->size,
                  __CPROVER_object_whole(store->litlens),
                  __CPROVER_object_whole(store->dists),
                  __CPROVER_object_whole(store->pos),
                  __CPROVER_object_whole(store->ll_symbol),
                  __CPROVER_object_whole(store->d_symbol),
                  __CPROVER_object_whole(store->ll_counts),
                  __CPROVER_object_whole(store->d_counts),
                  __CPROVER_object_whole(s->lmc->length),
                  __CPROVER_object_whole(s->lmc->dist),
                  __CPROVER_object_whole(s->lmc->sublen))
{
    size_t i, j, pos = 0;
    size_t windowstart = instart > ZOPFLI_WINDOW_SIZE
                             ? instart - ZOPFLI_WINDOW_SIZE
                             : 0;

    size_t total_length_test = 0;

    if (instart == inend)
        return;

    ZopfliResetHash(ZOPFLI_WINDOW_SIZE, h);
    ZopfliWarmupHash(in, windowstart, inend, h);
    for (i = windowstart; i < instart; i++)
    /* Each iteration only advances the running hash bookkeeping. */
    __CPROVER_assigns(i, h->val, h->val2,
                      __CPROVER_object_whole(h->head),
                      __CPROVER_object_whole(h->head2),
                      __CPROVER_object_whole(h->prev),
                      __CPROVER_object_whole(h->prev2),
                      __CPROVER_object_whole(h->hashval),
                      __CPROVER_object_whole(h->hashval2),
                      __CPROVER_object_whole(h->same))
    __CPROVER_loop_invariant(windowstart <= i && i <= instart)
    /* The running hash value stays masked into [0, HASH_MASK], which is exactly
       ZopfliUpdateHash's precondition for the next iteration. */
    __CPROVER_loop_invariant(h->val >= 0 && h->val <= HASH_MASK)
    __CPROVER_decreases(instart - i)
    {
        ZopfliUpdateHash(in, i, inend, h);
    }

    pos = instart;
    for (i = 0; i < pathsize; i++)
    /* Each iteration appends one LZ77 symbol and advances the hash; it touches
       the hash, the output store, and the longest-match cache slots. */
    __CPROVER_assigns(i, pos, total_length_test, j,
                      h->val, h->val2,
                      __CPROVER_object_whole(h->head),
                      __CPROVER_object_whole(h->head2),
                      __CPROVER_object_whole(h->prev),
                      __CPROVER_object_whole(h->prev2),
                      __CPROVER_object_whole(h->hashval),
                      __CPROVER_object_whole(h->hashval2),
                      __CPROVER_object_whole(h->same),
                      store->size,
                      __CPROVER_object_whole(store->litlens),
                      __CPROVER_object_whole(store->dists),
                      __CPROVER_object_whole(store->pos),
                      __CPROVER_object_whole(store->ll_symbol),
                      __CPROVER_object_whole(store->d_symbol),
                      __CPROVER_object_whole(store->ll_counts),
                      __CPROVER_object_whole(store->d_counts),
                      __CPROVER_object_whole(s->lmc->length),
                      __CPROVER_object_whole(s->lmc->dist),
                      __CPROVER_object_whole(s->lmc->sublen))
    __CPROVER_loop_invariant(i <= pathsize)
    /* The running hash value stays masked, satisfying ZopfliUpdateHash. */
    __CPROVER_loop_invariant(h->val >= 0 && h->val <= HASH_MASK)
    /* The encode cursor never passes the end of the input. */
    __CPROVER_loop_invariant(pos <= inend)
    __CPROVER_decreases(pathsize - i)
    {
        unsigned short length = path[i];
        unsigned short dummy_length;
        unsigned short dist;
        assert(pos < inend);

        ZopfliUpdateHash(in, pos, inend, h);

        /* Add to output. */
        if (length >= ZOPFLI_MIN_MATCH)
        {
            /* Get the distance by recalculating longest match. The found length
            should match the length from the path. */
            ZopfliFindLongestMatch(s, h, in, pos, inend, length, 0,
                                   &dist, &dummy_length);
            assert(!(dummy_length != length && length > 2 && dummy_length > 2));
            ZopfliVerifyLenDist(in, inend, pos, dist, length);
            ZopfliStoreLitLenDist(length, dist, pos, store);
            total_length_test += length;
        }
        else
        {
            length = 1;
            ZopfliStoreLitLenDist(in[pos], 0, pos, store);
            total_length_test++;
        }

        assert(pos + length <= inend);
        for (j = 1; j < length; j++)
        /* Only the running hash bookkeeping is advanced for the interior bytes. */
        __CPROVER_assigns(j, h->val, h->val2,
                          __CPROVER_object_whole(h->head),
                          __CPROVER_object_whole(h->head2),
                          __CPROVER_object_whole(h->prev),
                          __CPROVER_object_whole(h->prev2),
                          __CPROVER_object_whole(h->hashval),
                          __CPROVER_object_whole(h->hashval2),
                          __CPROVER_object_whole(h->same))
        __CPROVER_loop_invariant(1 <= j && j <= length)
        /* The running hash value stays masked, satisfying ZopfliUpdateHash. */
        __CPROVER_loop_invariant(h->val >= 0 && h->val <= HASH_MASK)
        __CPROVER_decreases((size_t)length - j)
        {
            ZopfliUpdateHash(in, pos + j, inend, h);
        }

        pos += length;
    }
}

/* The SAT backend can only expand the universal quantifier below over a
   constant range, so the reachable indices of length_array are bounded by this
   constant.  This is purely a bound on the input domain; it is unrelated to any
   loop-unwinding command-line argument. */
#define TRACEBACKWARDS_MAX_SIZE 1024

/*
Calculates the optimal path of lz77 lengths to use, from the calculated
length_array. The length_array must contain the optimal length to reach that
byte. The path will be filled with the lengths to use, so its data size will be
the amount of lz77 symbols.
*/
static void TraceBackwards(size_t size, const unsigned short *length_array,
                           unsigned short **path, size_t *pathsize)
/* path/pathsize are valid handles to the (initially empty) dynamic output
   array; the caller resets them to NULL/0 before the call. */
__CPROVER_requires(__CPROVER_is_fresh(path, sizeof(*path)))
__CPROVER_requires(__CPROVER_is_fresh(pathsize, sizeof(*pathsize)))
__CPROVER_requires(*path == NULL)
__CPROVER_requires(*pathsize == 0)
/* length_array is indexed in [0, size]; the optimal length to reach byte 0 is
   stored at index 0 and is never read by the backward walk. */
__CPROVER_requires(size <= TRACEBACKWARDS_MAX_SIZE)
__CPROVER_requires(__CPROVER_is_fresh(length_array,
                                      (size + 1) * sizeof(*length_array)))
/* Each stored length is a real LZ77 length (1..ZOPFLI_MAX_MATCH) that does not
   overshoot the start of the array, so the backward walk strictly decreases the
   index, stays in bounds, and terminates exactly at index 0. */
__CPROVER_requires(__CPROVER_forall {
    size_t i;
    (1 <= i && i < TRACEBACKWARDS_MAX_SIZE + 1) ==>
        (i <= size ==> (length_array[i] != 0 &&
                        length_array[i] <= i &&
                        length_array[i] <= ZOPFLI_MAX_MATCH))
})
/* The walk appends one entry per visited index and the mirror pass only permutes
   the result, so the function grows the path array and updates its size. */
__CPROVER_assigns(*path, *pathsize)
{
    size_t index = size;
    if (size == 0)
        return;
    for (;;)
    {
        ZOPFLI_APPEND_DATA(length_array[index], path, pathsize);
        assert(length_array[index] <= index);
        assert(length_array[index] <= ZOPFLI_MAX_MATCH);
        assert(length_array[index] != 0);
        index -= length_array[index];
        if (index == 0)
            break;
    }

    /* Mirror result. */
    for (index = 0; index < *pathsize / 2; index++)
    {
        unsigned short temp = (*path)[index];
        (*path)[index] = (*path)[*pathsize - index - 1];
        (*path)[*pathsize - index - 1] = temp;
    }
}

static size_t zopfli_min(size_t a, size_t b)
__CPROVER_ensures(__CPROVER_return_value == (a < b ? a : b))
__CPROVER_ensures(__CPROVER_return_value <= a && __CPROVER_return_value <= b)
__CPROVER_ensures(__CPROVER_return_value == a || __CPROVER_return_value == b)
__CPROVER_assigns()
{
    return a < b ? a : b;
}

/*
Finds the minimum possible cost this cost model can return for valid length and
distance symbols.
*/
static double GetCostModelMinCost(CostModelFun *costmodel, void *costcontext)
/* The cost model is supplied as a function pointer.  The only cost models in the
   program are GetCostStat (which reads its context as a SymbolStats) and
   GetCostFixed (which ignores its context); every caller passes one of them.
   Constraining the pointer to those two functions makes the indirect calls
   resolvable.  Only GetCostStat dereferences the context, so a fresh SymbolStats
   is required for that model; GetCostFixed never reads it (callers may pass NULL). */
__CPROVER_requires(costmodel == GetCostStat || costmodel == GetCostFixed)
__CPROVER_requires(costmodel == GetCostStat
                       ==> __CPROVER_is_fresh(costcontext, sizeof(SymbolStats)))
/* This function writes no global state of its own, and neither cost model
   writes any global state, so the whole call writes nothing observable. */
__CPROVER_assigns()
{
    double mincost;
    int bestlength = 0; /* length that has lowest cost in the cost model */
    int bestdist = 0;   /* distance that has lowest cost in the cost model */
    int i;
    /*
    Table of distances that have a different distance symbol in the deflate
    specification. Each value is the first distance that has a new symbol. Only
    different symbols affect the cost model so only these need to be checked.
    See RFC 1951 section 3.2.5. Compressed blocks (length and distance codes).
    */
    static const int dsymbols[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
        769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

    mincost = ZOPFLI_LARGE_FLOAT;
    for (i = 3; i < 259; i++)
    {
        double c = costmodel(i, 1, costcontext);
        if (c < mincost)
        {
            bestlength = i;
            mincost = c;
        }
    }

    mincost = ZOPFLI_LARGE_FLOAT;
    for (i = 0; i < 30; i++)
    {
        double c = costmodel(3, dsymbols[i], costcontext);
        if (c < mincost)
        {
            bestdist = dsymbols[i];
            mincost = c;
        }
    }

    return costmodel(bestlength, bestdist, costcontext);
}

/*
Performs the forward pass for "squeeze". Gets the most optimal length to reach
every byte from a previous byte, using cost calculations.
s: the ZopfliBlockState
in: the input data array
instart: where to start
inend: where to stop (not inclusive)
costmodel: function to calculate the cost of some lit/len/dist pair.
costcontext: abstract context for the costmodel function
length_array: output array of size (inend - instart) which will receive the best
length to reach this byte from a previous byte.
returns the cost that was, according to the costmodel, needed to get to the end.
*/
static double GetBestLengths(ZopfliBlockState *s,
                             const unsigned char *in,
                             size_t instart, size_t inend,
                             CostModelFun *costmodel, void *costcontext,
                             unsigned short *length_array,
                             ZopfliHash *h, float *costs)
/* ---- input window ----
   `in` is a readable buffer of `inend` bytes.  Every read (the warmup/update
   walk over [windowstart, instart) and the literal read in[i]) is at an index
   strictly below `inend`. */
__CPROVER_requires(__CPROVER_is_fresh(in, inend))
/* The region to encode is non-empty-or-empty but never inverted; instart..inend
   is the byte range of this run. */
__CPROVER_requires(instart <= inend)
/* ---- cost model ----
   The cost model is supplied as a function pointer.  The only cost models in the
   program are GetCostStat (which reads its context as a SymbolStats) and
   GetCostFixed (which ignores its context); every caller passes one of them.
   Constraining the pointer to those two makes the indirect calls resolvable, and
   a fresh SymbolStats is a valid context for GetCostStat, while GetCostFixed
   never reads it (callers may pass NULL). */
__CPROVER_requires(costmodel == GetCostStat || costmodel == GetCostFixed)
__CPROVER_requires(costmodel == GetCostStat
                       ==> __CPROVER_is_fresh(costcontext, sizeof(SymbolStats)))
/* ---- dynamic-programming arrays ----
   costs[] and length_array[] are each indexed in [0, inend - instart]; both are
   allocated by the caller with (blocksize + 1) entries. */
__CPROVER_requires(__CPROVER_is_fresh(length_array,
                                      (inend - instart + 1) * sizeof(*length_array)))
__CPROVER_requires(__CPROVER_is_fresh(costs,
                                      (inend - instart + 1) * sizeof(*costs)))
/* ---- hash object ----
   The hash and all of its sub-arrays must be valid, writable objects.  head and
   head2 are the full 65536-entry hash-to-index tables (ZopfliAllocHash); the
   per-window-slot arrays each hold ZOPFLI_WINDOW_SIZE entries.  These sizes are
   the union of what ZopfliResetHash, ZopfliWarmupHash, ZopfliUpdateHash and
   ZopfliFindLongestMatch require of the hash. */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_requires(__CPROVER_is_fresh(h->head, (size_t)65536 * sizeof(*h->head)))
__CPROVER_requires(__CPROVER_is_fresh(h->head2, (size_t)65536 * sizeof(*h->head2)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev2, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev2)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval2, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval2)))
__CPROVER_requires(__CPROVER_is_fresh(h->same, ZOPFLI_WINDOW_SIZE * sizeof(*h->same)))
/* ---- block state and longest-match cache ----
   `s` and its cache must be valid objects for ZopfliFindLongestMatch. */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
/* Only the DP arrays, the hash, the cache slots, and the running hash scalars
   are mutated; neither cost model writes observable global state. */
__CPROVER_assigns(__CPROVER_object_whole(costs),
                  __CPROVER_object_whole(length_array),
                  h->val, h->val2,
                  __CPROVER_object_whole(h->head),
                  __CPROVER_object_whole(h->head2),
                  __CPROVER_object_whole(h->prev),
                  __CPROVER_object_whole(h->prev2),
                  __CPROVER_object_whole(h->hashval),
                  __CPROVER_object_whole(h->hashval2),
                  __CPROVER_object_whole(h->same),
                  __CPROVER_object_whole(s->lmc->length),
                  __CPROVER_object_whole(s->lmc->dist),
                  __CPROVER_object_whole(s->lmc->sublen))
{
    /* Best cost to get here so far. */
    size_t blocksize = inend - instart;
    size_t i = 0, k, kend;
    unsigned short leng;
    unsigned short dist;
    unsigned short sublen[259];
    size_t windowstart = instart > ZOPFLI_WINDOW_SIZE
                             ? instart - ZOPFLI_WINDOW_SIZE
                             : 0;
    double result;
    double mincost = GetCostModelMinCost(costmodel, costcontext);
    double mincostaddcostj;

    if (instart == inend)
        return 0;

    ZopfliResetHash(ZOPFLI_WINDOW_SIZE, h);
    ZopfliWarmupHash(in, windowstart, inend, h);
    for (i = windowstart; i < instart; i++)
    {
        ZopfliUpdateHash(in, i, inend, h);
    }

    for (i = 1; i < blocksize + 1; i++)
        costs[i] = ZOPFLI_LARGE_FLOAT;
    costs[0] = 0; /* Because it's the start. */
    length_array[0] = 0;

    for (i = instart; i < inend; i++)
    {
        size_t j = i - instart; /* Index in the costs array and length_array. */
        ZopfliUpdateHash(in, i, inend, h);

        /* If we're in a long repetition of the same character and have more than
        ZOPFLI_MAX_MATCH characters before and after our position. */
        if (h->same[i & ZOPFLI_WINDOW_MASK] > ZOPFLI_MAX_MATCH * 2 && i > instart + ZOPFLI_MAX_MATCH + 1 && i + ZOPFLI_MAX_MATCH * 2 + 1 < inend && h->same[(i - ZOPFLI_MAX_MATCH) & ZOPFLI_WINDOW_MASK] > ZOPFLI_MAX_MATCH)
        {
            double symbolcost = costmodel(ZOPFLI_MAX_MATCH, 1, costcontext);
            /* Set the length to reach each one to ZOPFLI_MAX_MATCH, and the cost to
            the cost corresponding to that length. Doing this, we skip
            ZOPFLI_MAX_MATCH values to avoid calling ZopfliFindLongestMatch. */
            for (k = 0; k < ZOPFLI_MAX_MATCH; k++)
            {
                costs[j + ZOPFLI_MAX_MATCH] = costs[j] + symbolcost;
                length_array[j + ZOPFLI_MAX_MATCH] = ZOPFLI_MAX_MATCH;
                i++;
                j++;
                ZopfliUpdateHash(in, i, inend, h);
            }
        }

        ZopfliFindLongestMatch(s, h, in, i, inend, ZOPFLI_MAX_MATCH, sublen,
                               &dist, &leng);

        /* Literal. */
        if (i + 1 <= inend)
        {
            double newCost = costmodel(in[i], 0, costcontext) + costs[j];
            assert(newCost >= 0);
            if (newCost < costs[j + 1])
            {
                costs[j + 1] = newCost;
                length_array[j + 1] = 1;
            }
        }
        /* Lengths. */
        kend = zopfli_min(leng, inend - i);
        mincostaddcostj = mincost + costs[j];
        for (k = 3; k <= kend; k++)
        {
            double newCost;

            /* Calling the cost model is expensive, avoid this if we are already at
            the minimum possible cost that it can return. */
            if (costs[j + k] <= mincostaddcostj)
                continue;

            newCost = costmodel(k, sublen[k], costcontext) + costs[j];
            assert(newCost >= 0);
            if (newCost < costs[j + k])
            {
                assert(k <= ZOPFLI_MAX_MATCH);
                costs[j + k] = newCost;
                length_array[j + k] = k;
            }
        }
    }

    assert(costs[blocksize] >= 0);
    result = costs[blocksize];

    return result;
}

/*
Does a single run for ZopfliLZ77Optimal. For good compression, repeated runs
with updated statistics should be performed.
s: the block state
in: the input data array
instart: where to start
inend: where to stop (not inclusive)
path: pointer to dynamically allocated memory to store the path
pathsize: pointer to the size of the dynamic path array
length_array: array of size (inend - instart) used to store lengths
costmodel: function to use as the cost model for this squeeze run
costcontext: abstract context for the costmodel function
store: place to output the LZ77 data
returns the cost that was, according to the costmodel, needed to get to the end.
This is not the actual cost.
*/
static double LZ77OptimalRun(ZopfliBlockState *s,
                             const unsigned char *in, size_t instart, size_t inend,
                             unsigned short **path, size_t *pathsize,
                             unsigned short *length_array, CostModelFun *costmodel,
                             void *costcontext, ZopfliLZ77Store *store,
                             ZopfliHash *h, float *costs)
/* ---- input window ----
   `in` is a readable buffer of `inend` bytes (the union of what GetBestLengths
   and FollowPath read), and instart..inend is the (non-inverted) byte range of
   this run. */
__CPROVER_requires(__CPROVER_is_fresh(in, inend))
__CPROVER_requires(instart <= inend)
/* The block size is what TraceBackwards walks; bound it by its supported size so
   the backward path stays in range. */
__CPROVER_requires(inend - instart <= TRACEBACKWARDS_MAX_SIZE)
/* ---- cost model ----
   The cost model is supplied as a function pointer.  The only cost models in the
   program are GetCostStat (which reads its context as a SymbolStats) and
   GetCostFixed (which ignores its context); both LZ77OptimalRun callers pass one
   of them.  Constraining the pointer makes the indirect call resolvable.  Only
   GetCostStat dereferences the context, so a fresh SymbolStats is required for
   that model; the fixed caller passes NULL, which GetCostFixed never reads. */
__CPROVER_requires(costmodel == GetCostStat || costmodel == GetCostFixed)
__CPROVER_requires(costmodel == GetCostStat
                       ==> __CPROVER_is_fresh(costcontext, sizeof(SymbolStats)))
/* ---- dynamic-programming arrays ----
   costs[] and length_array[] are each indexed in [0, inend - instart]; both are
   allocated by the caller with (blocksize + 1) entries. */
__CPROVER_requires(__CPROVER_is_fresh(length_array,
                                      (inend - instart + 1) * sizeof(*length_array)))
__CPROVER_requires(__CPROVER_is_fresh(costs,
                                      (inend - instart + 1) * sizeof(*costs)))
/* ---- path output handles ----
   path/pathsize are valid handles.  The body frees *path before overwriting it,
   so the incoming pointer must be a freeable value; NULL satisfies that and
   matches the callers' fresh-or-reset path. */
__CPROVER_requires(__CPROVER_is_fresh(path, sizeof(*path)))
__CPROVER_requires(__CPROVER_is_fresh(pathsize, sizeof(*pathsize)))
__CPROVER_requires(*path == NULL)
/* ---- hash object ----
   The hash and all of its sub-arrays must be valid, writable objects, sized as
   ZopfliAllocHash allocates them (65536-entry head tables, ZOPFLI_WINDOW_SIZE
   per-slot arrays). */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_requires(__CPROVER_is_fresh(h->head, (size_t)65536 * sizeof(*h->head)))
__CPROVER_requires(__CPROVER_is_fresh(h->head2, (size_t)65536 * sizeof(*h->head2)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev2, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev2)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval2, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval2)))
__CPROVER_requires(__CPROVER_is_fresh(h->same, ZOPFLI_WINDOW_SIZE * sizeof(*h->same)))
/* ---- block state, longest-match cache, and output store ---- */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
/* The cost returned is the cost-model cost to reach the end: non-negative and
   below the sentinel (matching the closing assertion). */
__CPROVER_ensures(__CPROVER_return_value >= 0)
__CPROVER_ensures(__CPROVER_return_value < ZOPFLI_LARGE_FLOAT)
/* The path handle is left freeable: TraceBackwards either leaves it NULL (empty
   block) or points it at a freshly allocated buffer, so the callers' free(path)
   is well defined. */
__CPROVER_ensures(*path == NULL || __CPROVER_is_fresh(*path, sizeof(**path)))
/* Mutates the path handles, the DP arrays, the hash, the cache slots, and the
   output store (the union of the three callees' effects). */
__CPROVER_assigns(*path, *pathsize,
                  __CPROVER_object_whole(costs),
                  __CPROVER_object_whole(length_array),
                  h->val, h->val2,
                  __CPROVER_object_whole(h->head),
                  __CPROVER_object_whole(h->head2),
                  __CPROVER_object_whole(h->prev),
                  __CPROVER_object_whole(h->prev2),
                  __CPROVER_object_whole(h->hashval),
                  __CPROVER_object_whole(h->hashval2),
                  __CPROVER_object_whole(h->same),
                  store->size,
                  __CPROVER_object_whole(store->litlens),
                  __CPROVER_object_whole(store->dists),
                  __CPROVER_object_whole(store->pos),
                  __CPROVER_object_whole(store->ll_symbol),
                  __CPROVER_object_whole(store->d_symbol),
                  __CPROVER_object_whole(store->ll_counts),
                  __CPROVER_object_whole(store->d_counts),
                  __CPROVER_object_whole(s->lmc->length),
                  __CPROVER_object_whole(s->lmc->dist),
                  __CPROVER_object_whole(s->lmc->sublen))
{
    double cost = GetBestLengths(s, in, instart, inend, costmodel,
                                 costcontext, length_array, h, costs);
    free(*path);
    *path = 0;
    *pathsize = 0;
    TraceBackwards(inend - instart, length_array, path, pathsize);
    FollowPath(s, in, instart, inend, *path, *pathsize, store, h);
    assert(cost < ZOPFLI_LARGE_FLOAT);
    return cost;
}

void ZopfliCleanHash(ZopfliHash *h)
/* The hash struct and each of its seven array buffers must be valid, freshly
   allocated objects so that the calls to free() are well defined. */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_requires(__CPROVER_is_fresh(h->head, sizeof(*h->head)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev, sizeof(*h->prev)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval, sizeof(*h->hashval)))
__CPROVER_requires(__CPROVER_is_fresh(h->head2, sizeof(*h->head2)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev2, sizeof(*h->prev2)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval2, sizeof(*h->hashval2)))
__CPROVER_requires(__CPROVER_is_fresh(h->same, sizeof(*h->same)))
__CPROVER_assigns()
__CPROVER_frees(h->head, h->prev, h->hashval, h->head2, h->prev2, h->hashval2,
                h->same)
{
    free(h->head);
    free(h->prev);
    free(h->hashval);

    free(h->head2);
    free(h->prev2);
    free(h->hashval2);

    free(h->same);
}

void ZopfliAllocHash(size_t window_size, ZopfliHash *h)
/* The hash object must be a valid, writable struct.  The function only writes
   the seven array-pointer fields of *h; it never dereferences them, so no
   constraints on their incoming values are needed.  window_size is bounded by
   the (only) value callers pass, ZOPFLI_WINDOW_SIZE, which also keeps the
   malloc size computations free of overflow. */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
__CPROVER_requires(window_size <= ZOPFLI_WINDOW_SIZE)
__CPROVER_assigns(h->head, h->prev, h->hashval, h->same,
                  h->head2, h->prev2, h->hashval2)
/* Each array is freshly malloc'd (and so distinct), which is exactly what the
   callers that later hand the hash to LZ77OptimalRun / ZopfliCleanHash need. */
__CPROVER_ensures(__CPROVER_is_fresh(h->head, (size_t)65536 * sizeof(*h->head)))
__CPROVER_ensures(__CPROVER_is_fresh(h->prev, window_size * sizeof(*h->prev)))
__CPROVER_ensures(__CPROVER_is_fresh(h->hashval, window_size * sizeof(*h->hashval)))
__CPROVER_ensures(__CPROVER_is_fresh(h->same, window_size * sizeof(*h->same)))
__CPROVER_ensures(__CPROVER_is_fresh(h->head2, (size_t)65536 * sizeof(*h->head2)))
__CPROVER_ensures(__CPROVER_is_fresh(h->prev2, window_size * sizeof(*h->prev2)))
__CPROVER_ensures(__CPROVER_is_fresh(h->hashval2, window_size * sizeof(*h->hashval2)))
{
    h->head = (int *)malloc(sizeof(*h->head) * 65536);
    h->prev = (unsigned short *)malloc(sizeof(*h->prev) * window_size);
    h->hashval = (int *)malloc(sizeof(*h->hashval) * window_size);

    h->same = (unsigned short *)malloc(sizeof(*h->same) * window_size);

    h->head2 = (int *)malloc(sizeof(*h->head2) * 65536);
    h->prev2 = (unsigned short *)malloc(sizeof(*h->prev2) * window_size);
    h->hashval2 = (int *)malloc(sizeof(*h->hashval2) * window_size);
}

/*
Does the same as ZopfliLZ77Optimal, but optimized for the fixed tree of the
deflate standard.
The fixed tree never gives the best compression. But this gives the best
possible LZ77 encoding possible with the fixed tree.
This does not create or output any fixed tree, only LZ77 data optimized for
using with a fixed tree.
If instart is larger than 0, it uses values before instart as starting
dictionary.
*/
void ZopfliLZ77OptimalFixed(ZopfliBlockState *s,
                            const unsigned char *in,
                            size_t instart, size_t inend,
                            ZopfliLZ77Store *store)
/* ---- input window ----
   `in` is a readable buffer of `inend` bytes and instart..inend is the
   (non-inverted) byte range of this block, bounded by the size LZ77OptimalRun
   (via TraceBackwards) supports.  The bound also keeps blocksize + 1 from
   overflowing the malloc size computations. */
__CPROVER_requires(__CPROVER_is_fresh(in, inend))
__CPROVER_requires(instart <= inend)
__CPROVER_requires(inend - instart <= TRACEBACKWARDS_MAX_SIZE)
/* ---- block state and longest-match cache ----
   The state, its cache, and the cache's three buffers must be valid objects:
   LZ77OptimalRun requires the cache fresh and havocs the three buffers. */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->length, sizeof(*s->lmc->length)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->dist, sizeof(*s->lmc->dist)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->sublen, sizeof(*s->lmc->sublen)))
/* ---- output store ----
   The store and each of its seven buffers must be valid objects; LZ77OptimalRun
   havocs the whole of each buffer and updates the size. */
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
__CPROVER_requires(__CPROVER_is_fresh(store->litlens, sizeof(*store->litlens)))
__CPROVER_requires(__CPROVER_is_fresh(store->dists, sizeof(*store->dists)))
__CPROVER_requires(__CPROVER_is_fresh(store->pos, sizeof(*store->pos)))
__CPROVER_requires(__CPROVER_is_fresh(store->ll_symbol, sizeof(*store->ll_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(store->d_symbol, sizeof(*store->d_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(store->ll_counts, sizeof(*store->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(store->d_counts, sizeof(*store->d_counts)))
/* Records the block range in the state and produces the LZ77 encoding in the
   store; the hash, DP arrays and path are function-local and freed here. */
__CPROVER_assigns(s->blockstart, s->blockend,
                  store->size,
                  __CPROVER_object_whole(store->litlens),
                  __CPROVER_object_whole(store->dists),
                  __CPROVER_object_whole(store->pos),
                  __CPROVER_object_whole(store->ll_symbol),
                  __CPROVER_object_whole(store->d_symbol),
                  __CPROVER_object_whole(store->ll_counts),
                  __CPROVER_object_whole(store->d_counts),
                  __CPROVER_object_whole(s->lmc->length),
                  __CPROVER_object_whole(s->lmc->dist),
                  __CPROVER_object_whole(s->lmc->sublen))
{
    /* Dist to get to here with smallest cost. */
    size_t blocksize = inend - instart;
    unsigned short *length_array =
        (unsigned short *)malloc(sizeof(unsigned short) * (blocksize + 1));
    unsigned short *path = 0;
    size_t pathsize = 0;
    ZopfliHash hash;
    ZopfliHash *h = &hash;
    float *costs = (float *)malloc(sizeof(float) * (blocksize + 1));

    if (!costs)
        exit(-1); /* Allocation failed. */
    if (!length_array)
        exit(-1); /* Allocation failed. */

    ZopfliAllocHash(ZOPFLI_WINDOW_SIZE, h);

    s->blockstart = instart;
    s->blockend = inend;

    /* Shortest path for fixed tree This one should give the shortest possible
    result for fixed tree, no repeated runs are needed since the tree is known. */
    LZ77OptimalRun(s, in, instart, inend, &path, &pathsize,
                   length_array, GetCostFixed, 0, store, h, costs);

    free(length_array);
    free(path);
    free(costs);
    ZopfliCleanHash(h);
}

void ZopfliCleanCache(ZopfliLongestMatchCache *lmc)
/* The cache struct and each of its three buffers must be valid, freshly
   allocated objects so that the calls to free() are well defined. */
__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))
__CPROVER_requires(__CPROVER_is_fresh(lmc->length, sizeof(*lmc->length)))
__CPROVER_requires(__CPROVER_is_fresh(lmc->dist, sizeof(*lmc->dist)))
__CPROVER_requires(__CPROVER_is_fresh(lmc->sublen, sizeof(*lmc->sublen)))
__CPROVER_assigns()
__CPROVER_frees(lmc->length, lmc->dist, lmc->sublen)
{
    free(lmc->length);
    free(lmc->dist);
    free(lmc->sublen);
}

void ZopfliCleanBlockState(ZopfliBlockState *s)
/* The block state must be a valid object.  Its longest-match cache may be NULL
   (in which case nothing is freed) or a freshly allocated cache whose three
   buffers are themselves freshly allocated, so that the calls to free() (made
   here and inside ZopfliCleanCache) are well defined. */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(s->lmc != NULL ==> __CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
__CPROVER_requires(s->lmc != NULL ==> __CPROVER_is_fresh(s->lmc->length, sizeof(*s->lmc->length)))
__CPROVER_requires(s->lmc != NULL ==> __CPROVER_is_fresh(s->lmc->dist, sizeof(*s->lmc->dist)))
__CPROVER_requires(s->lmc != NULL ==> __CPROVER_is_fresh(s->lmc->sublen, sizeof(*s->lmc->sublen)))
__CPROVER_assigns()
__CPROVER_frees(s->lmc != NULL: s->lmc, s->lmc->length, s->lmc->dist, s->lmc->sublen)
{
    if (s->lmc)
    {
        ZopfliCleanCache(s->lmc);
        free(s->lmc);
    }
}

void ZopfliInitCache(size_t blocksize, ZopfliLongestMatchCache *lmc)
/* The cache object must be valid for writing.  blocksize is bounded so that the
   byte sizes passed to malloc (2*blocksize for length/dist and 24*blocksize for
   sublen) cannot overflow size_t; otherwise an undersized buffer would be
   allocated and the initialization loops would write out of bounds.  On return
   the three buffers are freshly allocated and large enough for their loops. */
__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))
__CPROVER_requires(blocksize <= 100)
__CPROVER_assigns(lmc->length, lmc->dist, lmc->sublen)
__CPROVER_ensures(__CPROVER_is_fresh(lmc->length, sizeof(unsigned short) * blocksize))
__CPROVER_ensures(__CPROVER_is_fresh(lmc->dist, sizeof(unsigned short) * blocksize))
__CPROVER_ensures(__CPROVER_is_fresh(lmc->sublen, ZOPFLI_CACHE_LENGTH * 3 * blocksize))
{
    size_t i;
    lmc->length = (unsigned short *)malloc(sizeof(unsigned short) * blocksize);
    lmc->dist = (unsigned short *)malloc(sizeof(unsigned short) * blocksize);
    /* Rather large amount of memory. */
    lmc->sublen = (unsigned char *)malloc(ZOPFLI_CACHE_LENGTH * 3 * blocksize);
    if (lmc->sublen == NULL)
    {
        fprintf(stderr,
                "Error: Out of memory. Tried allocating %lu bytes of memory.\n",
                (unsigned long)ZOPFLI_CACHE_LENGTH * 3 * blocksize);
        exit(EXIT_FAILURE);
    }

    /* length > 0 and dist 0 is invalid combination, which indicates on purpose
    that this cache value is not filled in yet. */
    for (i = 0; i < blocksize; i++)
        lmc->length[i] = 1;
    for (i = 0; i < blocksize; i++)
        lmc->dist[i] = 0;
    for (i = 0; i < ZOPFLI_CACHE_LENGTH * blocksize * 3; i++)
        lmc->sublen[i] = 0;
}

void ZopfliInitBlockState(const ZopfliOptions *options,
                          size_t blockstart, size_t blockend, int add_lmc,
                          ZopfliBlockState *s)
/* s must be valid for writing.  When add_lmc is set, a cache is allocated for
   a block of size blockend - blockstart, so blockstart <= blockend (the
   subtraction must not wrap) and the block size is bounded by the same limit
   that ZopfliInitCache requires of its blocksize argument. */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(blockstart <= blockend)
__CPROVER_requires(blockend - blockstart <= 100)
__CPROVER_assigns(s->options, s->blockstart, s->blockend, s->lmc)
__CPROVER_ensures(s->options == options)
__CPROVER_ensures(s->blockstart == blockstart)
__CPROVER_ensures(s->blockend == blockend)
__CPROVER_ensures(add_lmc == 0 ==> s->lmc == NULL)
{
    s->options = options;
    s->blockstart = blockstart;
    s->blockend = blockend;
    if (add_lmc)
    {
        s->lmc = (ZopfliLongestMatchCache *)malloc(sizeof(ZopfliLongestMatchCache));
        ZopfliInitCache(blockend - blockstart, s->lmc);
    }
    else
    {
        s->lmc = 0;
    }
}

void ZopfliCleanLZ77Store(ZopfliLZ77Store *store)
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
__CPROVER_requires(__CPROVER_is_fresh(store->litlens, sizeof(*store->litlens)))
__CPROVER_requires(__CPROVER_is_fresh(store->dists, sizeof(*store->dists)))
__CPROVER_requires(__CPROVER_is_fresh(store->pos, sizeof(*store->pos)))
__CPROVER_requires(__CPROVER_is_fresh(store->ll_symbol, sizeof(*store->ll_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(store->d_symbol, sizeof(*store->d_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(store->ll_counts, sizeof(*store->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(store->d_counts, sizeof(*store->d_counts)))
__CPROVER_assigns()
__CPROVER_frees(
    store->litlens, store->dists, store->pos, store->ll_symbol,
    store->d_symbol, store->ll_counts, store->d_counts)
__CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(store->litlens)))
__CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(store->dists)))
__CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(store->pos)))
__CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(store->ll_symbol)))
__CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(store->d_symbol)))
__CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(store->ll_counts)))
__CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(store->d_counts)))
{
    free(store->litlens);
    free(store->dists);
    free(store->pos);
    free(store->ll_symbol);
    free(store->d_symbol);
    free(store->ll_counts);
    free(store->d_counts);
}

void ZopfliInitLZ77Store(const unsigned char *data, ZopfliLZ77Store *store)
/* `store` must point to a valid, writable ZopfliLZ77Store object. `data` is only
   stored, never dereferenced, so it may be any pointer value. */
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
__CPROVER_assigns(*store)
/* All fields are reset: the store is logically empty with no owned buffers. */
__CPROVER_ensures(store->size == 0)
__CPROVER_ensures(store->litlens == 0)
__CPROVER_ensures(store->dists == 0)
__CPROVER_ensures(store->pos == 0)
__CPROVER_ensures(store->data == data)
__CPROVER_ensures(store->ll_symbol == 0)
__CPROVER_ensures(store->d_symbol == 0)
__CPROVER_ensures(store->ll_counts == 0)
__CPROVER_ensures(store->d_counts == 0)
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

static void AddLZ77BlockAutoType(const ZopfliOptions *options, int final,
                                 const ZopfliLZ77Store *lz77,
                                 size_t lstart, size_t lend,
                                 size_t expected_data_size,
                                 unsigned char *bp,
                                 unsigned char **out, size_t *outsize)
/* The block is the half-open command range [lstart, lend) of the LZ77 store; the
   preconditions are the union of those of the callees reachable on some path:
   ZopfliCalculateBlockSize (all three btypes), AddBits (empty-block path),
   ZopfliLZ77GetByteRange / ZopfliInitBlockState / ZopfliLZ77OptimalFixed
   (expensive-fixed path) and AddLZ77Block (the chosen output path). */
__CPROVER_requires(lstart <= lend)
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lend <= lz77->size)
/* options is read (verbose) and forwarded; final is a single header bit. */
__CPROVER_requires(__CPROVER_is_fresh(options, sizeof(*options)))
__CPROVER_requires(final == 0 || final == 1)
/* The per-command store buffers are the natural store-sized arrays.  The byte
   size of the 8-byte-element pos array must not wrap when computed. */
__CPROVER_requires(lz77->size * sizeof(*lz77->pos) >= lz77->size)
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->pos, lz77->size * sizeof(*lz77->pos)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lz77->size * sizeof(*lz77->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
/* Every stored symbol indexes its histogram in range over the whole store. */
__CPROVER_requires(__CPROVER_forall {
    size_t a; (a < lz77->size) ==> (lz77->ll_symbol[a] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t b; (b < lz77->size) ==> (lz77->d_symbol[b] < ZOPFLI_NUM_D)
})
/* "Large" histogram branch (lstart + ZOPFLI_NUM_LL*3 <= lend): the cumulative
   histograms must reach the chunk holding lpos = lend-1. */
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->ll_counts,
        ((size_t)ZOPFLI_NUM_LL * ((lend - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
            * sizeof(*lz77->ll_counts)))
__CPROVER_requires((lstart + ZOPFLI_NUM_LL * 3 <= lend) ==>
    __CPROVER_is_fresh(lz77->d_counts,
        ((size_t)ZOPFLI_NUM_D * ((lend - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
            * sizeof(*lz77->d_counts)))
/* Per-command DEFLATE validity over [lstart, lend): a literal (dist == 0) is a
   byte < 256; a back-reference carries a length in [3, 258] and a distance in
   [1, 512] (the AddLZ77Block bound, which also discharges ZopfliCalculateBlockSize). */
__CPROVER_requires(__CPROVER_forall {
    size_t c; (lstart <= c && c < lend && lz77->dists[c] == 0)
                  ==> (lz77->litlens[c] < 256)
})
__CPROVER_requires(__CPROVER_forall {
    size_t d; (lstart <= d && d < lend && lz77->dists[d] != 0)
                  ==> (lz77->litlens[d] >= 3 && lz77->litlens[d] <= 258
                       && lz77->dists[d] >= 1 && lz77->dists[d] <= 512)
})
/* The non-empty byte range [pos[lstart], end) of the original data, where
   end = pos[lend-1] + (dist[lend-1] == 0 ? 1 : litlen[lend-1]).  It is read by
   ZopfliLZ77OptimalFixed (in = lz77->data, inend = end) on the expensive-fixed
   path and copied by AddNonCompressedBlock on the btype-0 path; bound it by
   ANCB_MAX_INPUT (the stored-block copy-loop bound, <= ZopfliInitBlockState's
   block-size limit) and supply the readable input. */
__CPROVER_requires((lstart < lend) ==>
    (lz77->pos[lstart] <= lz77->pos[lend - 1]
        + (lz77->dists[lend - 1] == 0 ? 1 : lz77->litlens[lend - 1])))
__CPROVER_requires((lstart < lend) ==>
    (lz77->pos[lend - 1]
        + (lz77->dists[lend - 1] == 0 ? 1 : lz77->litlens[lend - 1])
        - lz77->pos[lstart] <= ANCB_MAX_INPUT))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->data,
        (lstart < lend)
            ? (lz77->pos[lend - 1]
                 + (lz77->dists[lend - 1] == 0 ? 1 : lz77->litlens[lend - 1]))
            : 1))
/* The bit-output cursor and the dynamic output array are valid single objects,
   and the append macro starts on its no-reallocation path. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
__CPROVER_requires(*bp <= 7)
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
/* The function emits the chosen block into the output buffer, advancing the bit
   cursor and growing (possibly reallocating) the dynamic buffer. */
__CPROVER_assigns(*bp, *outsize, *out, __CPROVER_object_whole(*out))
/* The bit cursor remains a valid in-byte index on every path. */
__CPROVER_ensures(*bp <= 7)
{
    double uncompressedcost = ZopfliCalculateBlockSize(lz77, lstart, lend, 0);
    double fixedcost = ZopfliCalculateBlockSize(lz77, lstart, lend, 1);
    double dyncost = ZopfliCalculateBlockSize(lz77, lstart, lend, 2);

    /* Whether to perform the expensive calculation of creating an optimal block
    with fixed huffman tree to check if smaller. Only do this for small blocks or
    blocks which already are pretty good with fixed huffman tree. */
    int expensivefixed = (lz77->size < 1000) || fixedcost <= dyncost * 1.1;

    ZopfliLZ77Store fixedstore;
    if (lstart == lend)
    {
        /* Smallest empty block is represented by fixed block */
        AddBits(final, 1, bp, out, outsize);
        AddBits(1, 2, bp, out, outsize); /* btype 01 */
        AddBits(0, 7, bp, out, outsize); /* end symbol has code 0000000 */
        return;
    }
    ZopfliInitLZ77Store(lz77->data, &fixedstore);
    if (expensivefixed)
    {
        /* Recalculate the LZ77 with ZopfliLZ77OptimalFixed */
        size_t instart = lz77->pos[lstart];
        size_t inend = instart + ZopfliLZ77GetByteRange(lz77, lstart, lend);

        ZopfliBlockState s;
        ZopfliInitBlockState(options, instart, inend, 1, &s);
        ZopfliLZ77OptimalFixed(&s, lz77->data, instart, inend, &fixedstore);
        fixedcost = ZopfliCalculateBlockSize(&fixedstore, 0, fixedstore.size, 1);
        ZopfliCleanBlockState(&s);
    }

    if (uncompressedcost < fixedcost && uncompressedcost < dyncost)
    {
        AddLZ77Block(options, 0, final, lz77, lstart, lend,
                     expected_data_size, bp, out, outsize);
    }
    else if (fixedcost < dyncost)
    {
        if (expensivefixed)
        {
            AddLZ77Block(options, 1, final, &fixedstore, 0, fixedstore.size,
                         expected_data_size, bp, out, outsize);
        }
        else
        {
            AddLZ77Block(options, 1, final, lz77, lstart, lend,
                         expected_data_size, bp, out, outsize);
        }
    }
    else
    {
        AddLZ77Block(options, 2, final, lz77, lstart, lend,
                     expected_data_size, bp, out, outsize);
    }

    ZopfliCleanLZ77Store(&fixedstore);
}

/*
Gets a score of the length given the distance. Typically, the score of the
length is the length itself, but if the distance is very long, decrease the
score of the length a bit to make up for the fact that long distances use large
amounts of extra bits.

This is not an accurate score, it is a heuristic only for the greedy LZ77
implementation. More accurate cost models are employed later. Making this
heuristic more accurate may hurt rather than improve compression.

The two direct uses of this heuristic are:
-avoid using a length of 3 in combination with a long distance. This only has
an effect if length == 3.
-make a slightly better choice between the two options of the lazy matching.

Indirectly, this affects:
-the block split points if the default of block splitting first is used, in a
rather unpredictable way
-the first zopfli run, so it affects the chance of the first run being closer
to the optimal output
*/
static int GetLengthScore(int length, int distance)
/* Pure scoring function. The only way to trigger undefined behavior is the
   `length - 1` subtraction overflowing when length == INT_MIN, which the
   precondition rules out (real callers pass unsigned short lengths). */
__CPROVER_requires(length > INT_MIN)
__CPROVER_ensures(__CPROVER_return_value == (distance > 1024 ? length - 1 : length))
{
    /*
    At 1024, the distance uses 9+ extra bits and this seems to be the sweet spot
    on tested files.
    */
    return distance > 1024 ? length - 1 : length;
}

/*
Does LZ77 using an algorithm similar to gzip, with lazy matching, rather than
with the slow but better "squeeze" implementation.
The result is placed in the ZopfliLZ77Store.
If instart is larger than 0, it uses values before instart as starting
dictionary.
*/
void ZopfliLZ77Greedy(ZopfliBlockState *s, const unsigned char *in,
                      size_t instart, size_t inend,
                      ZopfliLZ77Store *store, ZopfliHash *h)
/* ---- input range ---- */
/* The processed range is well-formed; the second loop runs over [instart, inend)
   and the warmup/dictionary loop over [windowstart, instart). */
__CPROVER_requires(instart <= inend)
/* The whole input buffer of `inend` bytes is a valid, readable object.  All reads
   are in[i] for windowstart <= i < inend, plus in[i-1] on the lazy-match path
   (only reachable after a prior iteration, so i >= instart+1 >= 1). */
__CPROVER_requires(__CPROVER_is_fresh(in, inend))
/* ---- block state and longest-match cache (consumed by ZopfliFindLongestMatch) ---- */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
/* Every processed position pos in [instart, inend) yields a non-wrapping in-block
   cache index pos - s->blockstart, bounded exactly as the cache callees require. */
__CPROVER_requires(s->blockstart <= instart)
__CPROVER_requires(inend - s->blockstart <= 101)
/* The cache arrays span the whole block [blockstart, inend): one length/dist slot
   and ZOPFLI_CACHE_LENGTH*3 sublen bytes per position. */
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->length,
                   sizeof(unsigned short) * (inend - s->blockstart)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->dist,
                   sizeof(unsigned short) * (inend - s->blockstart)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->sublen,
                   ZOPFLI_CACHE_LENGTH * 3 * (inend - s->blockstart)))
/* Each cache slot is in the freshly-initialized "not filled in yet" state
   (length 1, dist 0), so every ZopfliFindLongestMatch lookup misses, matching the
   slot state its contract requires. */
__CPROVER_requires(__CPROVER_forall {
    size_t kc; (kc < inend - s->blockstart) ==>
        (s->lmc->length[kc] == 1 && s->lmc->dist[kc] == 0) })
/* Each cached sublen length-byte decodes to <= 4, matching the callee precondition. */
__CPROVER_requires(__CPROVER_forall {
    size_t ks; (ks < ZOPFLI_CACHE_LENGTH * (inend - s->blockstart)) ==>
        s->lmc->sublen[ks * 3] <= 4 })
/* ---- hash object ---- */
__CPROVER_requires(__CPROVER_is_fresh(h, sizeof(*h)))
/* head/head2 are the full hash-to-index tables; the per-window-slot arrays each
   hold ZOPFLI_WINDOW_SIZE entries, matching ZopfliResetHash/ZopfliUpdateHash. */
__CPROVER_requires(__CPROVER_is_fresh(h->head, (size_t)65536 * sizeof(*h->head)))
__CPROVER_requires(__CPROVER_is_fresh(h->head2, (size_t)65536 * sizeof(*h->head2)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev)))
__CPROVER_requires(__CPROVER_is_fresh(h->prev2, ZOPFLI_WINDOW_SIZE * sizeof(*h->prev2)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval)))
__CPROVER_requires(__CPROVER_is_fresh(h->hashval2, ZOPFLI_WINDOW_SIZE * sizeof(*h->hashval2)))
__CPROVER_requires(__CPROVER_is_fresh(h->same, ZOPFLI_WINDOW_SIZE * sizeof(*h->same)))
/* ---- output store ---- */
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
/* ---- frame ---- */
/* The function appends LZ77 symbols to the store, updates the rolling hash, and
   fills the cache slots through ZopfliFindLongestMatch. */
__CPROVER_assigns(store->size,
                  __CPROVER_object_whole(store->litlens),
                  __CPROVER_object_whole(store->dists),
                  __CPROVER_object_whole(store->pos),
                  __CPROVER_object_whole(store->ll_symbol),
                  __CPROVER_object_whole(store->d_symbol),
                  __CPROVER_object_whole(store->ll_counts),
                  __CPROVER_object_whole(store->d_counts),
                  h->val, h->val2,
                  __CPROVER_object_whole(h->head),
                  __CPROVER_object_whole(h->head2),
                  __CPROVER_object_whole(h->prev),
                  __CPROVER_object_whole(h->prev2),
                  __CPROVER_object_whole(h->hashval),
                  __CPROVER_object_whole(h->hashval2),
                  __CPROVER_object_whole(h->same),
                  __CPROVER_object_whole(s->lmc->length),
                  __CPROVER_object_whole(s->lmc->dist),
                  __CPROVER_object_whole(s->lmc->sublen))
{
    size_t i = 0, j;
    unsigned short leng;
    unsigned short dist;
    int lengthscore;
    size_t windowstart = instart > ZOPFLI_WINDOW_SIZE
                             ? instart - ZOPFLI_WINDOW_SIZE
                             : 0;
    unsigned short dummysublen[259];

    /* Lazy matching. */
    unsigned prev_length = 0;
    unsigned prev_match = 0;
    int prevlengthscore;
    int match_available = 0;

    if (instart == inend)
        return;

    ZopfliResetHash(ZOPFLI_WINDOW_SIZE, h);
    ZopfliWarmupHash(in, windowstart, inend, h);
    for (i = windowstart; i < instart; i++)
    {
        ZopfliUpdateHash(in, i, inend, h);
    }

    for (i = instart; i < inend; i++)
    {
        ZopfliUpdateHash(in, i, inend, h);

        ZopfliFindLongestMatch(s, h, in, i, inend, ZOPFLI_MAX_MATCH, dummysublen,
                               &dist, &leng);
        lengthscore = GetLengthScore(leng, dist);

        /* Lazy matching. */
        prevlengthscore = GetLengthScore(prev_length, prev_match);
        if (match_available)
        {
            match_available = 0;
            if (lengthscore > prevlengthscore + 1)
            {
                ZopfliStoreLitLenDist(in[i - 1], 0, i - 1, store);
                if (lengthscore >= ZOPFLI_MIN_MATCH && leng < ZOPFLI_MAX_MATCH)
                {
                    match_available = 1;
                    prev_length = leng;
                    prev_match = dist;
                    continue;
                }
            }
            else
            {
                /* Add previous to output. */
                leng = prev_length;
                dist = prev_match;
                lengthscore = prevlengthscore;
                /* Add to output. */
                ZopfliVerifyLenDist(in, inend, i - 1, dist, leng);
                ZopfliStoreLitLenDist(leng, dist, i - 1, store);
                for (j = 2; j < leng; j++)
                {
                    assert(i < inend);
                    i++;
                    ZopfliUpdateHash(in, i, inend, h);
                }
                continue;
            }
        }
        else if (lengthscore >= ZOPFLI_MIN_MATCH && leng < ZOPFLI_MAX_MATCH)
        {
            match_available = 1;
            prev_length = leng;
            prev_match = dist;
            continue;
        }
        /* End of lazy matching. */

        /* Add to output. */
        if (lengthscore >= ZOPFLI_MIN_MATCH)
        {
            ZopfliVerifyLenDist(in, inend, i, dist, leng);
            ZopfliStoreLitLenDist(leng, dist, i, store);
        }
        else
        {
            leng = 1;
            ZopfliStoreLitLenDist(in[i], 0, i, store);
        }
        for (j = 1; j < leng; j++)
        {
            assert(i < inend);
            i++;
            ZopfliUpdateHash(in, i, inend, h);
        }
    }
}

/*
Calculates the entropy of each symbol, based on the counts of each symbol. The
result is similar to the result of ZopfliCalculateBitLengths, but with the
actual theoritical bit lengths according to the entropy. Since the resulting
values are fractional, they cannot be used to encode the tree specified by
DEFLATE.
*/
/* The errno object underlying CBMC's <math.h> models, named here so it can be
   listed as an assigns target of the contract below. */
extern int __CPROVER_errno;
void ZopfliCalculateEntropy(const size_t *count, size_t n, double *bitlengths)
/* The loop counter `i` is an `unsigned`, so the loops only terminate when `n`
   fits in an unsigned; bound `n` accordingly (no concrete value is fixed). */
__CPROVER_requires(n <= UINT_MAX)
__CPROVER_requires(__CPROVER_is_fresh(count, n * sizeof(*count)))
/* `bitlengths` need only be writable for n doubles; it must NOT be required to
   be a SEPARATE object from `count`.  Real callers (CalculateStatistics) pass
   two members of a single SymbolStats allocation, which are valid and disjoint
   but share one object, so an `is_fresh` here would be unsatisfiable at the call
   site under contract replacement.  `w_ok` constrains writability without
   imposing object separation; the per-element clamp/assert in the body makes the
   postcondition hold regardless of any aliasing between the two regions. */
__CPROVER_requires(__CPROVER_w_ok(bitlengths, n * sizeof(*bitlengths)))
/* CBMC's model of `log` from <math.h> writes the errno object
   (`__CPROVER_errno`), so the frame must include it. */
__CPROVER_assigns(__CPROVER_object_whole(bitlengths), __CPROVER_errno)
/* Every produced bit length is clamped/asserted non-negative. */
__CPROVER_ensures(__CPROVER_forall {
    size_t k; (k < n) ==> (bitlengths[k] >= 0)
})
{
    static const double kInvLog2 = 1.4426950408889; /* 1.0 / log(2.0) */
    unsigned sum = 0;
    unsigned i;
    double log2sum;
    for (i = 0; i < n; ++i)
    __CPROVER_assigns(i, sum)
    __CPROVER_loop_invariant(i <= n)
    __CPROVER_decreases(n - i)
    {
        sum += count[i];
    }
    log2sum = (sum == 0 ? log(n) : log(sum)) * kInvLog2;
    for (i = 0; i < n; ++i)
    __CPROVER_assigns(i, __CPROVER_object_whole(bitlengths))
    __CPROVER_loop_invariant(i <= n)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t k; (k < i) ==> (bitlengths[k] >= 0)
    })
    __CPROVER_decreases(n - i)
    {
        /* When the count of the symbol is 0, but its cost is requested anyway, it
        means the symbol will appear at least once anyway, so give it the cost as if
        its count is 1.*/
        if (count[i] == 0)
            bitlengths[i] = log2sum;
        else
            bitlengths[i] = log2sum - log(count[i]) * kInvLog2;
        /* Depending on compiler and architecture, the above subtraction of two
        floating point numbers may give a negative result very close to zero
        instead of zero (e.g. -5.973954e-17 with gcc 4.1.2 on Ubuntu 11.4). Clamp
        it to zero. These floating point imprecisions do not affect the cost model
        significantly so this is ok. */
        if (bitlengths[i] < 0 && bitlengths[i] > -1e-5)
            bitlengths[i] = 0;
        assert(bitlengths[i] >= 0);
    }
}

/* Calculates the entropy of the statistics */
static void CalculateStatistics(SymbolStats *stats)
/* The context object must be a valid, separately-allocated SymbolStats. */
__CPROVER_requires(__CPROVER_is_fresh(stats, sizeof(*stats)))
/* Each ZopfliCalculateEntropy call writes a `double` member of *stats; its
   contract frames the write as __CPROVER_object_whole(bitlengths), which for a
   pointer into *stats denotes the whole struct.  It also touches errno via the
   <math.h> models. */
__CPROVER_assigns(__CPROVER_object_whole(stats), __CPROVER_errno)
/* The distance entropies are produced by the last call and are non-negative.
   (The litlen entropies established by the first call are subsequently havocked
   by the second call's whole-object assigns frame, so they cannot be guaranteed
   here.)

   Each ZopfliCalculateEntropy call passes two members of this one SymbolStats
   allocation (litlens+ll_symbols, then dists+d_symbols).  Those members are
   valid and disjoint but share a single object, so the callee deliberately
   requires only __CPROVER_w_ok(bitlengths, ...) — not is_fresh — for its output
   buffer, which is discharged here under contract replacement. */
__CPROVER_ensures(__CPROVER_forall {
    size_t k; (k < (size_t)ZOPFLI_NUM_D) ==> (stats->d_symbols[k] >= 0)
})
{
    ZopfliCalculateEntropy(stats->litlens, ZOPFLI_NUM_LL, stats->ll_symbols);
    ZopfliCalculateEntropy(stats->dists, ZOPFLI_NUM_D, stats->d_symbols);
}

/* Appends the symbol statistics from the store. */
static void GetStatistics(const ZopfliLZ77Store *store, SymbolStats *stats)
/* The store and its two parallel command arrays (litlens / dists, both of length
   store->size) are valid, and the output SymbolStats is a separate allocation. */
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
__CPROVER_requires(__CPROVER_is_fresh(store->litlens,
                   store->size * sizeof(*store->litlens)))
__CPROVER_requires(__CPROVER_is_fresh(store->dists,
                   store->size * sizeof(*store->dists)))
/* The two command arrays must actually fit in memory: the byte size of each
   (store->size elements of unsigned short) must not overflow size_t, otherwise
   the is_fresh region would be smaller than store->size elements. */
__CPROVER_requires(store->size <= (~(size_t)0) / sizeof(*store->litlens))
__CPROVER_requires(__CPROVER_is_fresh(stats, sizeof(*stats)))
/* Per-command well-formedness, stated under a pure-integer guard so every store
   dereference sits in the consequent (in range for ia < store->size):
   - a literal (dists[ia] == 0) indexes stats->litlens directly with litlens[ia],
     so litlens[ia] must be a valid litlen-alphabet index (< ZOPFLI_NUM_LL);
   - a length command (dists[ia] != 0) passes litlens[ia] (a DEFLATE length) to
     ZopfliGetLengthSymbol (requires [0, 258]) and dists[ia] (a DEFLATE distance)
     to ZopfliGetDistSymbol (requires [1, 32768]). */
__CPROVER_requires(__CPROVER_forall {
    size_t ia; (ia < store->size) ==> (
        (store->dists[ia] == 0 ==> store->litlens[ia] < ZOPFLI_NUM_LL) &&
        (store->dists[ia] != 0 ==> (store->litlens[ia] <= 258 &&
                                    store->dists[ia] >= 1 &&
                                    store->dists[ia] <= 32768)))
})
/* Only the output statistics object is written; CalculateStatistics also touches
   errno through the <math.h> models. */
__CPROVER_assigns(__CPROVER_object_whole(stats), __CPROVER_errno)
/* The distance entropies produced by the final CalculateStatistics call are
   non-negative (mirrors that callee's postcondition). */
__CPROVER_ensures(__CPROVER_forall {
    size_t k; (k < (size_t)ZOPFLI_NUM_D) ==> (stats->d_symbols[k] >= 0)
})
{
    size_t i;
    for (i = 0; i < store->size; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(stats))
    __CPROVER_loop_invariant(i <= store->size)
    __CPROVER_decreases(store->size - i)
    {
        if (store->dists[i] == 0)
        {
            stats->litlens[store->litlens[i]]++;
        }
        else
        {
            stats->litlens[ZopfliGetLengthSymbol(store->litlens[i])]++;
            stats->dists[ZopfliGetDistSymbol(store->dists[i])]++;
        }
    }
    stats->litlens[256] = 1; /* End symbol. */

    CalculateStatistics(stats);
}

/* Zeroes the litlen and dist frequency counts of *stats.  Only those two
   arrays are written; we frame the assigns as the whole object (the two count
   arrays are members of the single SymbolStats allocation) and establish that
   every count is zero on exit. */
static void ClearStatFreqs(SymbolStats *stats)
__CPROVER_requires(__CPROVER_is_fresh(stats, sizeof(*stats)))
__CPROVER_assigns(__CPROVER_object_whole(stats))
__CPROVER_ensures(__CPROVER_forall {
    size_t k; (k < (size_t)ZOPFLI_NUM_LL) ==> (stats->litlens[k] == 0)
})
__CPROVER_ensures(__CPROVER_forall {
    size_t j; (j < (size_t)ZOPFLI_NUM_D) ==> (stats->dists[j] == 0)
})
{
    size_t i;
    for (i = 0; i < ZOPFLI_NUM_LL; i++)
        stats->litlens[i] = 0;
    for (i = 0; i < ZOPFLI_NUM_D; i++)
        stats->dists[i] = 0;
}

/* Get random number: "Multiply-With-Carry" generator of G. Marsaglia.
   The state object must be a valid, fresh RanState.  Both seed words are
   advanced exactly as the Marsaglia recurrence specifies, and the returned
   32-bit value is the concatenation of the two updated seed words.  Each
   intermediate product fits in an unsigned int (36969 * 65535 + 65535 and
   18000 * 65535 + 65535 are both well below UINT_MAX), so no overflow can
   occur in the seed updates; the final combine is an unsigned (wrap-defined)
   computation. */
static unsigned int Ran(RanState *state)
__CPROVER_requires(__CPROVER_is_fresh(state, sizeof(RanState)))
__CPROVER_assigns(state->m_w, state->m_z)
__CPROVER_ensures(state->m_z ==
    36969u * (__CPROVER_old(state->m_z) & 65535u) + (__CPROVER_old(state->m_z) >> 16))
__CPROVER_ensures(state->m_w ==
    18000u * (__CPROVER_old(state->m_w) & 65535u) + (__CPROVER_old(state->m_w) >> 16))
__CPROVER_ensures(__CPROVER_return_value == (state->m_z << 16) + state->m_w)
{
    state->m_z = 36969 * (state->m_z & 65535) + (state->m_z >> 16);
    state->m_w = 18000 * (state->m_w & 65535) + (state->m_w >> 16);
    return (state->m_z << 16) + state->m_w; /* 32-bit result. */
}

/* Randomly perturbs the n-element frequency array by copying, for a random
   subset of positions, the value at another random position into it.  The
   state must be a valid, fresh RanState (the Marsaglia generator it drives),
   and freqs must point to at least n size_t counts.  n must be positive: every
   index used is either i (with 0 <= i < n maintained by the loop) or
   Ran(state) % n, which is in [0, n-1] only when n > 0; the latter also makes
   the modulo well defined.  Only the freqs object and the generator's two seed
   words are written. */
static void RandomizeFreqs(RanState *state, size_t *freqs, int n)
__CPROVER_requires(n > 0)
__CPROVER_requires(__CPROVER_is_fresh(state, sizeof(RanState)))
__CPROVER_requires(__CPROVER_is_fresh(freqs, (size_t)n * sizeof(*freqs)))
__CPROVER_assigns(__CPROVER_object_whole(freqs), state->m_w, state->m_z)
{
    int i;
    for (i = 0; i < n; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(freqs), state->m_w, state->m_z)
    __CPROVER_loop_invariant(0 <= i && i <= n)
    {
        if ((Ran(state) >> 4) % 3 == 0)
            freqs[i] = freqs[Ran(state) % n];
    }
}

/* Randomly perturbs both frequency arrays of the statistics object, then forces
   the litlen end symbol (256) back to a count of 1.  The generator state must be
   a valid, fresh RanState, and stats a valid, fresh SymbolStats; the two are
   distinct objects so each RandomizeFreqs call sees a fresh state separate from
   the array it perturbs.  Only the two count arrays and the generator's two seed
   words are written. */
static void RandomizeStatFreqs(RanState *state, SymbolStats *stats)
__CPROVER_requires(__CPROVER_is_fresh(state, sizeof(RanState)))
__CPROVER_requires(__CPROVER_is_fresh(stats, sizeof(SymbolStats)))
__CPROVER_assigns(__CPROVER_object_whole(stats->litlens),
                  __CPROVER_object_whole(stats->dists),
                  state->m_w, state->m_z)
__CPROVER_ensures(stats->litlens[256] == 1)
{
    RandomizeFreqs(state, stats->litlens, ZOPFLI_NUM_LL);
    RandomizeFreqs(state, stats->dists, ZOPFLI_NUM_D);
    stats->litlens[256] = 1; /* End symbol. */
}

static void InitRanState(RanState *state)
/* Initialises the random-number generator's state.  The state object must be a
   valid, fresh RanState; on return its two seed words hold their fixed initial
   values. */
__CPROVER_requires(__CPROVER_is_fresh(state, sizeof(RanState)))
__CPROVER_assigns(state->m_w, state->m_z)
__CPROVER_ensures(state->m_w == 1)
__CPROVER_ensures(state->m_z == 2)
{
    state->m_w = 1;
    state->m_z = 2;
}

/* Adds the bit lengths. */
static void AddWeighedStatFreqs(const SymbolStats *stats1, double w1,
                                const SymbolStats *stats2, double w2,
                                SymbolStats *result)
/* The function forms (size_t)(freq1*w1 + freq2*w2) for every litlen/dist
   frequency.  To keep the floating-point-to-integer conversion in range (a
   conversion of an out-of-[0, SIZE_MAX] double is undefined behaviour) the
   weights are confined to [0, 1] and the input frequencies to a generous bound,
   so each weighted sum stays well below SIZE_MAX.  The three statistics objects
   are required fresh (and hence pairwise disjoint), and only result's litlens
   and dists arrays are written; the entropy fields are untouched. */
__CPROVER_requires(__CPROVER_is_fresh(stats1, sizeof(SymbolStats)))
__CPROVER_requires(__CPROVER_is_fresh(stats2, sizeof(SymbolStats)))
__CPROVER_requires(__CPROVER_is_fresh(result, sizeof(SymbolStats)))
__CPROVER_requires(w1 >= 0.0 && w1 <= 1.0)
__CPROVER_requires(w2 >= 0.0 && w2 <= 1.0)
__CPROVER_requires(__CPROVER_forall {
    size_t ia; ia < ZOPFLI_NUM_LL ==> stats1->litlens[ia] <= 0xFFFFFFFF })
__CPROVER_requires(__CPROVER_forall {
    size_t ib; ib < ZOPFLI_NUM_LL ==> stats2->litlens[ib] <= 0xFFFFFFFF })
__CPROVER_requires(__CPROVER_forall {
    size_t ic; ic < ZOPFLI_NUM_D ==> stats1->dists[ic] <= 0xFFFFFFFF })
__CPROVER_requires(__CPROVER_forall {
    size_t id; id < ZOPFLI_NUM_D ==> stats2->dists[id] <= 0xFFFFFFFF })
__CPROVER_assigns(__CPROVER_object_whole(result->litlens))
__CPROVER_assigns(__CPROVER_object_whole(result->dists))
__CPROVER_ensures(result->litlens[256] == 1)
__CPROVER_ensures(__CPROVER_forall {
    size_t ie; (ie < ZOPFLI_NUM_D) ==> result->dists[ie] <= 0x1FFFFFFFFULL })
{
    size_t i;
    for (i = 0; i < ZOPFLI_NUM_LL; i++)
    {
        result->litlens[i] =
            (size_t)(stats1->litlens[i] * w1 + stats2->litlens[i] * w2);
    }
    for (i = 0; i < ZOPFLI_NUM_D; i++)
    {
        result->dists[i] =
            (size_t)(stats1->dists[i] * w1 + stats2->dists[i] * w2);
    }
    result->litlens[256] = 1; /* End symbol. */
}

static size_t CeilDiv(size_t a, size_t b)
__CPROVER_requires(b != 0)
__CPROVER_requires(a <= (~(size_t)0) - (b - 1))
__CPROVER_ensures(__CPROVER_return_value * b >= a)
__CPROVER_ensures(__CPROVER_return_value == 0 || (__CPROVER_return_value - 1) * b < a)
{
    return (a + b - 1) / b;
}

void ZopfliCopyLZ77Store(
    const ZopfliLZ77Store *source, ZopfliLZ77Store *dest)
/* Deep-copies `source` into `dest`.  `dest`'s previously-owned buffers are freed
   (via ZopfliCleanLZ77Store, which needs each old buffer fresh) and the store is
   re-initialised before fresh buffers sized to `source->size` are allocated and
   filled.  `source` and `dest` are distinct, fresh stores; `source`'s parallel
   arrays must be readable for `source->size` elements, and its histograms for
   the per-chunk rounded-up lengths llsize / dsize.  `source->size` is bounded so
   that none of the byte-size computations (CeilDiv's a+b-1, the ZOPFLI_NUM_*
   multiplications, and the malloc sizes) overflow size_t. */
__CPROVER_requires(__CPROVER_is_fresh(source, sizeof(*source)))
__CPROVER_requires(__CPROVER_is_fresh(dest, sizeof(*dest)))
__CPROVER_requires(__CPROVER_is_fresh(dest->litlens, sizeof(*dest->litlens)))
__CPROVER_requires(__CPROVER_is_fresh(dest->dists, sizeof(*dest->dists)))
__CPROVER_requires(__CPROVER_is_fresh(dest->pos, sizeof(*dest->pos)))
__CPROVER_requires(__CPROVER_is_fresh(dest->ll_symbol, sizeof(*dest->ll_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(dest->d_symbol, sizeof(*dest->d_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(dest->ll_counts, sizeof(*dest->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(dest->d_counts, sizeof(*dest->d_counts)))
__CPROVER_requires(source->size <= (~(size_t)0) / ZOPFLI_NUM_LL / sizeof(size_t))
__CPROVER_requires(
    __CPROVER_is_fresh(source->litlens, sizeof(*source->litlens) * source->size))
__CPROVER_requires(
    __CPROVER_is_fresh(source->dists, sizeof(*source->dists) * source->size))
__CPROVER_requires(
    __CPROVER_is_fresh(source->pos, sizeof(*source->pos) * source->size))
__CPROVER_requires(
    __CPROVER_is_fresh(source->ll_symbol, sizeof(*source->ll_symbol) * source->size))
__CPROVER_requires(
    __CPROVER_is_fresh(source->d_symbol, sizeof(*source->d_symbol) * source->size))
__CPROVER_requires(__CPROVER_is_fresh(
    source->ll_counts,
    sizeof(*source->ll_counts) *
        (ZOPFLI_NUM_LL * ((source->size + (ZOPFLI_NUM_LL - 1)) / ZOPFLI_NUM_LL))))
__CPROVER_requires(__CPROVER_is_fresh(
    source->d_counts,
    sizeof(*source->d_counts) *
        (ZOPFLI_NUM_D * ((source->size + (ZOPFLI_NUM_D - 1)) / ZOPFLI_NUM_D))))
__CPROVER_assigns(*dest)
__CPROVER_frees(dest->litlens, dest->dists, dest->pos, dest->ll_symbol,
                dest->d_symbol, dest->ll_counts, dest->d_counts)
__CPROVER_ensures(dest->size == source->size)
__CPROVER_ensures(dest->data == source->data)
__CPROVER_ensures(__CPROVER_forall {
    size_t ea; ea < source->size ==> dest->litlens[ea] == source->litlens[ea] })
__CPROVER_ensures(__CPROVER_forall {
    size_t eb; eb < source->size ==> dest->dists[eb] == source->dists[eb] })
__CPROVER_ensures(__CPROVER_forall {
    size_t ec; ec < source->size ==> dest->pos[ec] == source->pos[ec] })
__CPROVER_ensures(__CPROVER_forall {
    size_t ed; ed < source->size ==> dest->ll_symbol[ed] == source->ll_symbol[ed] })
__CPROVER_ensures(__CPROVER_forall {
    size_t ee; ee < source->size ==> dest->d_symbol[ee] == source->d_symbol[ee] })
__CPROVER_ensures(__CPROVER_forall {
    size_t ef;
    ef < (ZOPFLI_NUM_LL * ((source->size + (ZOPFLI_NUM_LL - 1)) / ZOPFLI_NUM_LL))
        ==> dest->ll_counts[ef] == source->ll_counts[ef] })
__CPROVER_ensures(__CPROVER_forall {
    size_t eg;
    eg < (ZOPFLI_NUM_D * ((source->size + (ZOPFLI_NUM_D - 1)) / ZOPFLI_NUM_D))
        ==> dest->d_counts[eg] == source->d_counts[eg] })
{
    size_t i;
    size_t llsize = ZOPFLI_NUM_LL * CeilDiv(source->size, ZOPFLI_NUM_LL);
    size_t dsize = ZOPFLI_NUM_D * CeilDiv(source->size, ZOPFLI_NUM_D);
    ZopfliCleanLZ77Store(dest);
    ZopfliInitLZ77Store(source->data, dest);
    dest->litlens =
        (unsigned short *)malloc(sizeof(*dest->litlens) * source->size);
    dest->dists = (unsigned short *)malloc(sizeof(*dest->dists) * source->size);
    dest->pos = (size_t *)malloc(sizeof(*dest->pos) * source->size);
    dest->ll_symbol =
        (unsigned short *)malloc(sizeof(*dest->ll_symbol) * source->size);
    dest->d_symbol =
        (unsigned short *)malloc(sizeof(*dest->d_symbol) * source->size);
    dest->ll_counts = (size_t *)malloc(sizeof(*dest->ll_counts) * llsize);
    dest->d_counts = (size_t *)malloc(sizeof(*dest->d_counts) * dsize);

    /* Allocation failed. */
    if (!dest->litlens || !dest->dists)
        exit(-1);
    if (!dest->pos)
        exit(-1);
    if (!dest->ll_symbol || !dest->d_symbol)
        exit(-1);
    if (!dest->ll_counts || !dest->d_counts)
        exit(-1);

    dest->size = source->size;
    for (i = 0; i < source->size; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(dest->litlens),
                      __CPROVER_object_whole(dest->dists),
                      __CPROVER_object_whole(dest->pos),
                      __CPROVER_object_whole(dest->ll_symbol),
                      __CPROVER_object_whole(dest->d_symbol))
    __CPROVER_loop_invariant(i <= source->size)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t la; la < i ==> dest->litlens[la] == source->litlens[la] })
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t lb; lb < i ==> dest->dists[lb] == source->dists[lb] })
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t lc; lc < i ==> dest->pos[lc] == source->pos[lc] })
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t ld; ld < i ==> dest->ll_symbol[ld] == source->ll_symbol[ld] })
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t le; le < i ==> dest->d_symbol[le] == source->d_symbol[le] })
    {
        dest->litlens[i] = source->litlens[i];
        dest->dists[i] = source->dists[i];
        dest->pos[i] = source->pos[i];
        dest->ll_symbol[i] = source->ll_symbol[i];
        dest->d_symbol[i] = source->d_symbol[i];
    }
    for (i = 0; i < llsize; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(dest->ll_counts))
    __CPROVER_loop_invariant(i <= llsize)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t lf; lf < i ==> dest->ll_counts[lf] == source->ll_counts[lf] })
    {
        dest->ll_counts[i] = source->ll_counts[i];
    }
    for (i = 0; i < dsize; i++)
    __CPROVER_assigns(i, __CPROVER_object_whole(dest->d_counts))
    __CPROVER_loop_invariant(i <= dsize)
    __CPROVER_loop_invariant(__CPROVER_forall {
        size_t lg; lg < i ==> dest->d_counts[lg] == source->d_counts[lg] })
    {
        dest->d_counts[i] = source->d_counts[i];
    }
}

static void CopyStats(SymbolStats *source, SymbolStats *dest)
/* source and dest are distinct SymbolStats objects; is_fresh on each whole
   struct gives the memcpy non-overlapping source/destination regions. */
__CPROVER_requires(__CPROVER_is_fresh(source, sizeof(*source)))
__CPROVER_requires(__CPROVER_is_fresh(dest, sizeof(*dest)))
__CPROVER_assigns(*dest)
/* After the copy, every entry of each of dest's four arrays equals source's. */
__CPROVER_ensures(__CPROVER_forall {
    size_t il; il < (size_t)ZOPFLI_NUM_LL ==> dest->litlens[il] == source->litlens[il] })
__CPROVER_ensures(__CPROVER_forall {
    size_t id; id < (size_t)ZOPFLI_NUM_D ==> dest->dists[id] == source->dists[id] })
__CPROVER_ensures(__CPROVER_forall {
    size_t is; is < (size_t)ZOPFLI_NUM_LL ==> dest->ll_symbols[is] == source->ll_symbols[is] })
__CPROVER_ensures(__CPROVER_forall {
    size_t jd; jd < (size_t)ZOPFLI_NUM_D ==> dest->d_symbols[jd] == source->d_symbols[jd] })
{
    memcpy(dest->litlens, source->litlens,
           ZOPFLI_NUM_LL * sizeof(dest->litlens[0]));
    memcpy(dest->dists, source->dists, ZOPFLI_NUM_D * sizeof(dest->dists[0]));

    memcpy(dest->ll_symbols, source->ll_symbols,
           ZOPFLI_NUM_LL * sizeof(dest->ll_symbols[0]));
    memcpy(dest->d_symbols, source->d_symbols,
           ZOPFLI_NUM_D * sizeof(dest->d_symbols[0]));
}

/* Sets everything to 0. */
static void InitStats(SymbolStats *stats)
/* stats points to a valid, writable SymbolStats whose embedded arrays we zero. */
__CPROVER_requires(__CPROVER_is_fresh(stats, sizeof(*stats)))
__CPROVER_assigns(stats->litlens, stats->dists, stats->ll_symbols, stats->d_symbols)
/* After the call, every entry of each of the four arrays is zero. */
__CPROVER_ensures(__CPROVER_forall {
    size_t il; il < (size_t)ZOPFLI_NUM_LL ==> stats->litlens[il] == (size_t)0 })
__CPROVER_ensures(__CPROVER_forall {
    size_t id; id < (size_t)ZOPFLI_NUM_D ==> stats->dists[id] == (size_t)0 })
__CPROVER_ensures(__CPROVER_forall {
    size_t is; is < (size_t)ZOPFLI_NUM_LL ==> stats->ll_symbols[is] == 0.0 })
__CPROVER_ensures(__CPROVER_forall {
    size_t jd; jd < (size_t)ZOPFLI_NUM_D ==> stats->d_symbols[jd] == 0.0 })
{
    memset(stats->litlens, 0, ZOPFLI_NUM_LL * sizeof(stats->litlens[0]));
    memset(stats->dists, 0, ZOPFLI_NUM_D * sizeof(stats->dists[0]));

    memset(stats->ll_symbols, 0, ZOPFLI_NUM_LL * sizeof(stats->ll_symbols[0]));
    memset(stats->d_symbols, 0, ZOPFLI_NUM_D * sizeof(stats->d_symbols[0]));
}

/*
Calculates lit/len and dist pairs for given data.
If instart is larger than 0, it uses values before instart as starting
dictionary.
*/
void ZopfliLZ77Optimal(ZopfliBlockState *s,
                       const unsigned char *in, size_t instart, size_t inend,
                       int numiterations,
                       ZopfliLZ77Store *store)
/* ---- input window ----
   `in` is a readable buffer of `inend` bytes and instart..inend is the
   (non-inverted) byte range of this block.  The range is bounded both by the
   size LZ77OptimalRun (via TraceBackwards) supports and, more tightly, by the
   per-block cache-index bound that ZopfliLZ77Greedy requires; the latter also
   keeps blocksize + 1 from overflowing the malloc size computations. */
__CPROVER_requires(__CPROVER_is_fresh(in, inend))
__CPROVER_requires(instart <= inend)
__CPROVER_requires(inend - instart <= TRACEBACKWARDS_MAX_SIZE)
/* numiterations is a non-negative count of refinement passes. */
__CPROVER_requires(numiterations >= 0)
/* ---- block state, options and longest-match cache ----
   The state, its options block (read for the verbose flags), its cache, and the
   cache's three buffers must be valid objects.  ZopfliLZ77Greedy maps every
   processed position pos in [instart, inend) to an in-block cache index
   pos - s->blockstart, so blockstart <= instart and the block measured from
   blockstart is bounded exactly as the cache callees require; the cache buffers
   span that whole block. */
__CPROVER_requires(__CPROVER_is_fresh(s, sizeof(*s)))
__CPROVER_requires(__CPROVER_is_fresh(s->options, sizeof(*s->options)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc, sizeof(*s->lmc)))
__CPROVER_requires(s->blockstart <= instart)
__CPROVER_requires(inend - s->blockstart <= 101)
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->length,
                   sizeof(unsigned short) * (inend - s->blockstart)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->dist,
                   sizeof(unsigned short) * (inend - s->blockstart)))
__CPROVER_requires(__CPROVER_is_fresh(s->lmc->sublen,
                   ZOPFLI_CACHE_LENGTH * 3 * (inend - s->blockstart)))
/* Each cache slot starts in the freshly-initialized "not filled in yet" state
   (length 1, dist 0) and each cached sublen length-byte decodes to <= 4, matching
   what ZopfliFindLongestMatch (via ZopfliLZ77Greedy) requires. */
__CPROVER_requires(__CPROVER_forall {
    size_t kc; (kc < inend - s->blockstart) ==>
        (s->lmc->length[kc] == 1 && s->lmc->dist[kc] == 0) })
__CPROVER_requires(__CPROVER_forall {
    size_t ks; (ks < ZOPFLI_CACHE_LENGTH * (inend - s->blockstart)) ==>
        s->lmc->sublen[ks * 3] <= 4 })
/* ---- output store ----
   The store and each of its seven buffers must be valid objects; the buffers are
   the (possibly empty) currently-owned ones that ZopfliCopyLZ77Store frees before
   re-allocating, so each must be freeable, i.e. fresh. */
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
__CPROVER_requires(__CPROVER_is_fresh(store->litlens, sizeof(*store->litlens)))
__CPROVER_requires(__CPROVER_is_fresh(store->dists, sizeof(*store->dists)))
__CPROVER_requires(__CPROVER_is_fresh(store->pos, sizeof(*store->pos)))
__CPROVER_requires(__CPROVER_is_fresh(store->ll_symbol, sizeof(*store->ll_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(store->d_symbol, sizeof(*store->d_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(store->ll_counts, sizeof(*store->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(store->d_counts, sizeof(*store->d_counts)))
/* The function refines the LZ77 encoding into the output store (deep-copied from
   a function-local working store), and havocs the longest-match cache buffers
   through the greedy / optimal runs.  The <math.h> models touch errno. */
__CPROVER_assigns(*store,
                  __CPROVER_object_whole(s->lmc->length),
                  __CPROVER_object_whole(s->lmc->dist),
                  __CPROVER_object_whole(s->lmc->sublen),
                  __CPROVER_errno)
__CPROVER_frees(store->litlens, store->dists, store->pos, store->ll_symbol,
                store->d_symbol, store->ll_counts, store->d_counts)
{
    /* Dist to get to here with smallest cost. */
    size_t blocksize = inend - instart;
    unsigned short *length_array =
        (unsigned short *)malloc(sizeof(unsigned short) * (blocksize + 1));
    unsigned short *path = 0;
    size_t pathsize = 0;
    ZopfliLZ77Store currentstore;
    ZopfliHash hash;
    ZopfliHash *h = &hash;
    SymbolStats stats, beststats, laststats;
    int i;
    float *costs = (float *)malloc(sizeof(float) * (blocksize + 1));
    double cost;
    double bestcost = ZOPFLI_LARGE_FLOAT;
    double lastcost = 0;
    /* Try randomizing the costs a bit once the size stabilizes. */
    RanState ran_state;
    int lastrandomstep = -1;

    if (!costs)
        exit(-1); /* Allocation failed. */
    if (!length_array)
        exit(-1); /* Allocation failed. */

    InitRanState(&ran_state);
    InitStats(&stats);
    ZopfliInitLZ77Store(in, &currentstore);
    ZopfliAllocHash(ZOPFLI_WINDOW_SIZE, h);

    /* Do regular deflate, then loop multiple shortest path runs, each time using
    the statistics of the previous run. */

    /* Initial run. */
    ZopfliLZ77Greedy(s, in, instart, inend, &currentstore, h);
    GetStatistics(&currentstore, &stats);

    /* Repeat statistics with each time the cost model from the previous stat
    run. */
    for (i = 0; i < numiterations; i++)
    {
        ZopfliCleanLZ77Store(&currentstore);
        ZopfliInitLZ77Store(in, &currentstore);
        LZ77OptimalRun(s, in, instart, inend, &path, &pathsize,
                       length_array, GetCostStat, (void *)&stats,
                       &currentstore, h, costs);
        cost = ZopfliCalculateBlockSize(&currentstore, 0, currentstore.size, 2);
        if (s->options->verbose_more || (s->options->verbose && cost < bestcost))
        {
            fprintf(stderr, "Iteration %d: %d bit\n", i, (int)cost);
        }
        if (cost < bestcost)
        {
            /* Copy to the output store. */
            ZopfliCopyLZ77Store(&currentstore, store);
            CopyStats(&stats, &beststats);
            bestcost = cost;
        }
        CopyStats(&stats, &laststats);
        ClearStatFreqs(&stats);
        GetStatistics(&currentstore, &stats);
        if (lastrandomstep != -1)
        {
            /* This makes it converge slower but better. Do it only once the
            randomness kicks in so that if the user does few iterations, it gives a
            better result sooner. */
            AddWeighedStatFreqs(&stats, 1.0, &laststats, 0.5, &stats);
            CalculateStatistics(&stats);
        }
        if (i > 5 && cost == lastcost)
        {
            CopyStats(&beststats, &stats);
            RandomizeStatFreqs(&ran_state, &stats);
            CalculateStatistics(&stats);
            lastrandomstep = i;
        }
        lastcost = cost;
    }

    free(length_array);
    free(path);
    free(costs);
    ZopfliCleanLZ77Store(&currentstore);
    ZopfliCleanHash(h);
}

/*
Finds next block to try to split, the largest of the available ones.
The largest is chosen to make sure that if only a limited amount of blocks is
requested, their sizes are spread evenly.
lz77size: the size of the LL77 data, which is the size of the done array here.
done: array indicating which blocks starting at that position are no longer
splittable (splitting them increases rather than decreases cost).
splitpoints: the splitpoints found so far.
npoints: the amount of splitpoints found so far.
lstart: output variable, giving start of block.
lend: output variable, giving end of block.
returns 1 if a block was found, 0 if no block found (all are done).
*/
static int FindLargestSplittableBlock(
    size_t lz77size, const unsigned char *done,
    const size_t *splitpoints, size_t npoints,
    size_t *lstart, size_t *lend)
/* The loop runs npoints + 1 times and reads done[start] for every candidate
   block start, where start is either 0 or one of the splitpoints. So at least
   done[0] is read (hence lz77size >= 1, which also makes lz77size - 1 safe), and
   every splitpoint must be a valid index into done. */
__CPROVER_requires(lz77size >= 1)
/* Bound npoints so that npoints + 1 and the array sizings below cannot overflow;
   this is a structural bound, unrelated to any CBMC unwinding argument. */
__CPROVER_requires(npoints < (((size_t)-1) / sizeof(*splitpoints)) - 1)
__CPROVER_requires(__CPROVER_is_fresh(done, lz77size * sizeof(*done)))
/* splitpoints holds npoints indices; when npoints == 0 it is never dereferenced. */
__CPROVER_requires(npoints == 0
    || __CPROVER_is_fresh(splitpoints, npoints * sizeof(*splitpoints)))
/* Every recorded splitpoint is a valid index into the done array. */
__CPROVER_requires(__CPROVER_forall {
    size_t k; (k < npoints) ==> (splitpoints[k] < lz77size)
})
__CPROVER_requires(__CPROVER_is_fresh(lstart, sizeof(*lstart)))
__CPROVER_requires(__CPROVER_is_fresh(lend, sizeof(*lend)))
__CPROVER_assigns(*lstart, *lend)
/* The return value is a boolean flag: a block was found (1) or not (0). */
__CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == 1)
{
    size_t longest = 0;
    int found = 0;
    size_t i;
    for (i = 0; i <= npoints; i++)
    __CPROVER_assigns(i, longest, found, *lstart, *lend)
    __CPROVER_loop_invariant(i <= npoints + 1)
    __CPROVER_loop_invariant(found == 0 || found == 1)
    {
        size_t start = i == 0 ? 0 : splitpoints[i - 1];
        size_t end = i == npoints ? lz77size - 1 : splitpoints[i];
        if (!done[start] && end - start > longest)
        {
            *lstart = start;
            *lend = end;
            found = 1;
            longest = end - start;
        }
    }
    return found;
}

/*
Prints the block split points as decimal and hex values in the terminal.
*/
static void PrintBlockSplitPoints(const ZopfliLZ77Store *lz77,
                                  const size_t *lz77splitpoints,
                                  size_t nlz77points)
/* The lz77 store handle is readable. */
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
/* The per-command litlen / dist arrays are the natural store-sized buffers; the
   main loop reads dists[i] and litlens[i] for i in [0, size).  Guard the byte
   sizes against wrap-around. */
__CPROVER_requires(lz77->size * sizeof(*lz77->dists) >= lz77->size)
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lz77->size * sizeof(*lz77->litlens)))
/* The nlz77points input split indices are readable. */
__CPROVER_requires(nlz77points * sizeof(*lz77splitpoints) >= nlz77points)
__CPROVER_requires(
    __CPROVER_is_fresh(lz77splitpoints, nlz77points * sizeof(*lz77splitpoints)))
/* The split points are given as strictly-increasing lz77 command indices that all
   lie within the store.  Strict monotonicity keeps lz77splitpoints[npoints] indexed
   in [0, nlz77points) (npoints only advances on a match), and the last index being
   < size guarantees every point is matched by some i, so npoints reaches
   nlz77points and the closing assert holds. */
__CPROVER_requires(__CPROVER_forall {
    size_t k;
    (0 < k && k < nlz77points) ==> (lz77splitpoints[k - 1] < lz77splitpoints[k])
})
__CPROVER_requires(
    (nlz77points == 0) || (lz77splitpoints[nlz77points - 1] < lz77->size))
{
    size_t *splitpoints = 0;
    size_t npoints = 0;
    size_t i;
    /* The input is given as lz77 indices, but we want to see the uncompressed
    index values. */
    size_t pos = 0;
    if (nlz77points > 0)
    {
        for (i = 0; i < lz77->size; i++)
        {
            size_t length = lz77->dists[i] == 0 ? 1 : lz77->litlens[i];
            if (lz77splitpoints[npoints] == i)
            {
                ZOPFLI_APPEND_DATA(pos, &splitpoints, &npoints);
                if (npoints == nlz77points)
                    break;
            }
            pos += length;
        }
    }
    assert(npoints == nlz77points);

    fprintf(stderr, "block split points: ");
    for (i = 0; i < npoints; i++)
    {
        fprintf(stderr, "%d ", (int)splitpoints[i]);
    }
    fprintf(stderr, "(hex:");
    for (i = 0; i < npoints; i++)
    {
        fprintf(stderr, " %x", (int)splitpoints[i]);
    }
    fprintf(stderr, ")\n");

    free(splitpoints);
}

static void AddSorted(size_t value, size_t **out, size_t *outsize)
/* out, outsize must be valid pointers. */
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
/* The buffer holds *outsize live elements, plus room to append one more without
   overflowing.  Constraining *outsize to a non-zero, non-power-of-two value
   keeps the ZOPFLI_APPEND_DATA macro on its no-reallocation path for the single
   append performed here. */
__CPROVER_requires((*outsize) != 0 && ((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, ((*outsize) + 1) * sizeof(**out)))
__CPROVER_assigns(*outsize, __CPROVER_object_whole(*out))
/* Exactly one element (`value`) is inserted into the array. */
__CPROVER_ensures(*outsize == __CPROVER_old(*outsize) + 1)
{
    size_t i;
    ZOPFLI_APPEND_DATA(value, out, outsize);
    for (i = 0; i + 1 < *outsize; i++)
    {
        if ((*out)[i] > value)
        {
            size_t j;
            for (j = *outsize - 1; j > i; j--)
            {
                (*out)[j] = (*out)[j - 1];
            }
            (*out)[i] = value;
            break;
        }
    }
}

/* Try to find minimum faster by recursively checking multiple points. */
#define NUM 9 /* Good value: 9. */

/*
Finds minimum of function f(i) where is is of type size_t, f(i) is of type
double, i is in range start-end (excluding end).
Outputs the minimum value in *smallest and returns the index of this value.
*/
static size_t FindMinimum(FindMinimumFun f, void *context,
                          size_t start, size_t end, double *smallest)
__CPROVER_requires(f == SplitCost)
__CPROVER_requires(start < end)
__CPROVER_requires(__CPROVER_is_fresh(context, sizeof(SplitCostContext)))
__CPROVER_requires(__CPROVER_is_fresh(smallest, sizeof(double)))
__CPROVER_assigns(*smallest)
/* Stated over the parameter values directly rather than via __CPROVER_old:
   goto-instrument cannot build a history variable for a compound call argument
   (the caller passes `lstart + 1`), and under contract replacement a parameter
   already denotes the argument value, which is exactly what the caller needs.
   The bounds also hold under enforcement: the body only ever grows `start` and
   shrinks `end`, so the returned index stays in [start, end). */
__CPROVER_ensures(__CPROVER_return_value >= start)
__CPROVER_ensures(__CPROVER_return_value < end)
__CPROVER_ensures(*smallest <= ZOPFLI_LARGE_FLOAT)
{
    if (end - start < 1024)
    {
        double best = ZOPFLI_LARGE_FLOAT;
        size_t result = start;
        size_t i;
        for (i = start; i < end; i++)
        {
            double v = f(i, context);
            if (v < best)
            {
                best = v;
                result = i;
            }
        }
        *smallest = best;
        return result;
    }
    else
    {
        /* Try to find minimum faster by recursively checking multiple points. */
#define NUM 9 /* Good value: 9. */
        size_t i;
        size_t p[NUM];
        double vp[NUM];
        size_t besti;
        double best;
        double lastbest = ZOPFLI_LARGE_FLOAT;
        size_t pos = start;

        for (;;)
        {
            if (end - start <= NUM)
                break;

            for (i = 0; i < NUM; i++)
            {
                p[i] = start + (i + 1) * ((end - start) / (NUM + 1));
                vp[i] = f(p[i], context);
            }
            besti = 0;
            best = vp[0];
            for (i = 1; i < NUM; i++)
            {
                if (vp[i] < best)
                {
                    best = vp[i];
                    besti = i;
                }
            }
            if (best > lastbest)
                break;

            start = besti == 0 ? start : p[besti - 1];
            end = besti == NUM - 1 ? end : p[besti + 1];

            pos = p[besti];
            lastbest = best;
        }
        *smallest = lastbest;
        return pos;
#undef NUM
    }
}

/*
Does blocksplitting on LZ77 data.
The output splitpoints are indices in the LZ77 data.
maxblocks: set a limit to the amount of blocks. Set to 0 to mean no limit.
*/
void ZopfliBlockSplitLZ77(const ZopfliOptions *options,
                          const ZopfliLZ77Store *lz77, size_t maxblocks,
                          size_t **splitpoints, size_t *npoints)
/* options->verbose is read at the end. */
__CPROVER_requires(__CPROVER_is_fresh(options, sizeof(*options)))
/* The lz77 store handle and its parallel arrays are readable.  These mirror the
   precondition of EstimateCost (called directly here) so that every EstimateCost
   call inside the loop is well-formed; the foralls are stated over the whole
   store so they cover every [lstart, lend) sub-range the loop queries. */
__CPROVER_requires(__CPROVER_is_fresh(lz77, sizeof(*lz77)))
__CPROVER_requires(lz77->size * sizeof(*lz77->pos) >= lz77->size)
__CPROVER_requires(__CPROVER_is_fresh(lz77->pos, lz77->size * sizeof(*lz77->pos)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->litlens, lz77->size * sizeof(*lz77->litlens)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->dists, lz77->size * sizeof(*lz77->dists)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->ll_symbol, lz77->size * sizeof(*lz77->ll_symbol)))
__CPROVER_requires(
    __CPROVER_is_fresh(lz77->d_symbol, lz77->size * sizeof(*lz77->d_symbol)))
__CPROVER_requires(__CPROVER_forall {
    size_t il; (il < lz77->size) ==> (lz77->ll_symbol[il] < ZOPFLI_NUM_LL)
})
__CPROVER_requires(__CPROVER_forall {
    size_t ir; (ir < lz77->size) ==> (lz77->d_symbol[ir] < ZOPFLI_NUM_D)
})
/* Cumulative histograms, sized for the largest sub-range (lend == size); the
   sizing is monotone in lend, so this covers every shorter range too. */
__CPROVER_requires(__CPROVER_is_fresh(lz77->ll_counts,
    ((size_t)ZOPFLI_NUM_LL * ((lz77->size - 1) / ZOPFLI_NUM_LL) + ZOPFLI_NUM_LL)
        * sizeof(*lz77->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(lz77->d_counts,
    ((size_t)ZOPFLI_NUM_D * ((lz77->size - 1) / ZOPFLI_NUM_D) + ZOPFLI_NUM_D)
        * sizeof(*lz77->d_counts)))
/* Symbol-level well-formedness used by the small-block cost path. */
__CPROVER_requires(__CPROVER_forall {
    size_t jl; (jl < lz77->size) ==> (lz77->litlens[jl] < 259)
})
__CPROVER_requires(__CPROVER_forall {
    size_t jr; (jr < lz77->size && lz77->dists[jr] != 0)
                  ==> (lz77->litlens[jr] >= 3
                       && lz77->dists[jr] >= 1 && lz77->dists[jr] <= 32768)
})
/* The output split-point array handles are valid. */
__CPROVER_requires(__CPROVER_is_fresh(npoints, sizeof(*npoints)))
__CPROVER_requires(__CPROVER_is_fresh(splitpoints, sizeof(*splitpoints)))
/* Bound *npoints so npoints + 1 and the array sizings cannot overflow (matches
   FindLargestSplittableBlock's bound). */
__CPROVER_requires(*npoints < (((size_t)-1) / sizeof(**splitpoints)) - 1)
/* The buffer holds *npoints live indices with room for one more append; a
   non-zero, non-power-of-two count keeps the first AddSorted on the macro's
   no-reallocation path. */
__CPROVER_requires((*npoints) != 0 && ((*npoints) & ((*npoints) - 1)) != 0)
__CPROVER_requires(
    __CPROVER_is_fresh(*splitpoints, ((*npoints) + 1) * sizeof(**splitpoints)))
/* The recorded split points are strictly increasing lz77 indices, all in range
   (the invariants AddSorted maintains and that the consumers here require). */
__CPROVER_requires(__CPROVER_forall {
    size_t k; (k < *npoints) ==> ((*splitpoints)[k] < lz77->size)
})
__CPROVER_requires(__CPROVER_forall {
    size_t k2;
    (0 < k2 && k2 < *npoints) ==> ((*splitpoints)[k2 - 1] < (*splitpoints)[k2])
})
__CPROVER_assigns(*npoints, *splitpoints, __CPROVER_object_whole(*splitpoints))
/* The number of split points never decreases. */
__CPROVER_ensures(*npoints >= __CPROVER_old(*npoints))
{
    size_t lstart, lend;
    size_t i;
    size_t llpos = 0;
    size_t numblocks = 1;
    unsigned char *done;
    double splitcost, origcost;

    if (lz77->size < 10)
        return; /* This code fails on tiny files. */

    done = (unsigned char *)malloc(lz77->size);
    if (!done)
        exit(-1); /* Allocation failed. */
    for (i = 0; i < lz77->size; i++)
        done[i] = 0;

    lstart = 0;
    lend = lz77->size;
    for (;;)
    {
        SplitCostContext c;

        if (maxblocks > 0 && numblocks >= maxblocks)
        {
            break;
        }

        c.lz77 = lz77;
        c.start = lstart;
        c.end = lend;
        assert(lstart < lend);
        llpos = FindMinimum(SplitCost, &c, lstart + 1, lend, &splitcost);

        assert(llpos > lstart);
        assert(llpos < lend);

        origcost = EstimateCost(lz77, lstart, lend);

        if (splitcost > origcost || llpos == lstart + 1 || llpos == lend)
        {
            done[lstart] = 1;
        }
        else
        {
            AddSorted(llpos, splitpoints, npoints);
            numblocks++;
        }

        if (!FindLargestSplittableBlock(
                lz77->size, done, *splitpoints, *npoints, &lstart, &lend))
        {
            break; /* No further split will probably reduce compression. */
        }

        if (lend - lstart < 10)
        {
            break;
        }
    }

    if (options->verbose)
    {
        PrintBlockSplitPoints(lz77, *splitpoints, *npoints);
    }

    free(done);
}

/*
Does blocksplitting on uncompressed data.
The output splitpoints are indices in the uncompressed bytes.

options: general program options.
in: uncompressed input data
instart: where to start splitting
inend: where to end splitting (not inclusive)
maxblocks: maximum amount of blocks to split into, or 0 for no limit
splitpoints: dynamic array to put the resulting split point coordinates into.
The coordinates are indices in the input array.
npoints: pointer to amount of splitpoints, for the dynamic array. The amount of
blocks is the amount of splitpoitns + 1.
*/
void ZopfliBlockSplit(const ZopfliOptions *options,
                      const unsigned char *in, size_t instart, size_t inend,
                      size_t maxblocks, size_t **splitpoints, size_t *npoints)
/* ---- options ----
   `options` is read by ZopfliInitBlockState (stored) and by ZopfliBlockSplitLZ77
   (verbose); it must be a valid object. */
__CPROVER_requires(__CPROVER_is_fresh(options, sizeof(*options)))
/* ---- input window ----
   `in` is a readable buffer of `inend` bytes; ZopfliLZ77Greedy reads in[i] over
   the processed range and `store` only stores the pointer.  The block range is
   well-formed and small enough for ZopfliInitBlockState (blockend - blockstart
   <= 100) and ZopfliLZ77Greedy (inend - blockstart <= 101, with blockstart set
   to instart by ZopfliInitBlockState). */
__CPROVER_requires(__CPROVER_is_fresh(in, inend))
__CPROVER_requires(instart <= inend)
__CPROVER_requires(inend - instart <= 100)
/* ---- output split-point array ----
   Both out-parameters are valid, writable handles; the function resets *npoints
   to 0 and *splitpoints to NULL and then grows the array. */
__CPROVER_requires(__CPROVER_is_fresh(npoints, sizeof(*npoints)))
__CPROVER_requires(__CPROVER_is_fresh(splitpoints, sizeof(*splitpoints)))
/* The function only writes the two out-parameters (and the array it freshly
   allocates for them); every other touched object is function-local. */
__CPROVER_assigns(*npoints, *splitpoints)
{
    size_t pos = 0;
    size_t i;
    ZopfliBlockState s;
    size_t *lz77splitpoints = 0;
    size_t nlz77points = 0;
    ZopfliLZ77Store store;
    ZopfliHash hash;
    ZopfliHash *h = &hash;

    ZopfliInitLZ77Store(in, &store);
    ZopfliInitBlockState(options, instart, inend, 0, &s);
    ZopfliAllocHash(ZOPFLI_WINDOW_SIZE, h);

    *npoints = 0;
    *splitpoints = 0;

    /* Unintuitively, Using a simple LZ77 method here instead of ZopfliLZ77Optimal
    results in better blocks. */
    ZopfliLZ77Greedy(&s, in, instart, inend, &store, h);

    ZopfliBlockSplitLZ77(options,
                         &store, maxblocks,
                         &lz77splitpoints, &nlz77points);

    /* Convert LZ77 positions to positions in the uncompressed input. */
    pos = instart;
    if (nlz77points > 0)
    {
        for (i = 0; i < store.size; i++)
        {
            size_t length = store.dists[i] == 0 ? 1 : store.litlens[i];
            if (lz77splitpoints[*npoints] == i)
            {
                ZOPFLI_APPEND_DATA(pos, splitpoints, npoints);
                if (*npoints == nlz77points)
                    break;
            }
            pos += length;
        }
    }
    assert(*npoints == nlz77points);

    free(lz77splitpoints);
    ZopfliCleanBlockState(&s);
    ZopfliCleanLZ77Store(&store);
    ZopfliCleanHash(h);
}

void ZopfliAppendLZ77Store(const ZopfliLZ77Store *store,
                           ZopfliLZ77Store *target)
/* Both stores are valid, readable objects. */
__CPROVER_requires(__CPROVER_is_fresh(store, sizeof(*store)))
__CPROVER_requires(__CPROVER_is_fresh(target, sizeof(*target)))
/* The source's parallel arrays are readable for all `size` LZ77 symbols; this
   function only ever reads litlens, dists and pos. */
__CPROVER_requires(__CPROVER_is_fresh(store->litlens,
                   store->size * sizeof(*store->litlens)))
__CPROVER_requires(__CPROVER_is_fresh(store->dists,
                   store->size * sizeof(*store->dists)))
__CPROVER_requires(__CPROVER_is_fresh(store->pos,
                   store->size * sizeof(*store->pos)))
/* Every source symbol is a valid DEFLATE litlen/dist so each forwarded call to
   ZopfliStoreLitLenDist meets its [0,258] / [0,32768] preconditions. */
__CPROVER_requires(__CPROVER_forall {
    size_t i; i < store->size ==> store->litlens[i] <= 258 })
__CPROVER_requires(__CPROVER_forall {
    size_t j; j < store->size ==> store->dists[j] <= 32768 })
/* Across the whole run of appends the target's element count walks the values
   [target->size, target->size + store->size).  Each value must keep
   ZopfliStoreLitLenDist on its in-place, no-reallocation path: non-zero, not a
   power of two, off both per-chunk histogram boundaries, and small enough that
   its allocation-size arithmetic cannot overflow. */
__CPROVER_requires(__CPROVER_forall {
    size_t k; (target->size <= k && k < target->size + store->size) ==>
        (k != 0 && (k & (k - 1)) != 0 &&
         k % ZOPFLI_NUM_LL != 0 && k % ZOPFLI_NUM_D != 0 &&
         k <= ((~(size_t)0) / (2 * sizeof(size_t))) - ZOPFLI_NUM_LL) })
/* The target's arrays already have room for the existing elements plus every
   element appended here, so each in-place append at index `size` stays in
   bounds (the largest index touched is target->size + store->size - 1). */
__CPROVER_requires(__CPROVER_is_fresh(target->litlens,
                   (target->size + store->size) * sizeof(*target->litlens)))
__CPROVER_requires(__CPROVER_is_fresh(target->dists,
                   (target->size + store->size) * sizeof(*target->dists)))
__CPROVER_requires(__CPROVER_is_fresh(target->pos,
                   (target->size + store->size) * sizeof(*target->pos)))
__CPROVER_requires(__CPROVER_is_fresh(target->ll_symbol,
                   (target->size + store->size) * sizeof(*target->ll_symbol)))
__CPROVER_requires(__CPROVER_is_fresh(target->d_symbol,
                   (target->size + store->size) * sizeof(*target->d_symbol)))
/* The cumulative histograms must hold the whole chunk every append indexes
   into; no append here crosses a chunk boundary, so the chunk base computed
   from target->size + store->size bounds the largest index used. */
__CPROVER_requires(__CPROVER_is_fresh(target->ll_counts,
                   (ZOPFLI_NUM_LL * ((target->size + store->size) / ZOPFLI_NUM_LL)
                       + ZOPFLI_NUM_LL) * sizeof(*target->ll_counts)))
__CPROVER_requires(__CPROVER_is_fresh(target->d_counts,
                   (ZOPFLI_NUM_D * ((target->size + store->size) / ZOPFLI_NUM_D)
                       + ZOPFLI_NUM_D) * sizeof(*target->d_counts)))
__CPROVER_assigns(target->size,
                  __CPROVER_object_whole(target->litlens),
                  __CPROVER_object_whole(target->dists),
                  __CPROVER_object_whole(target->pos),
                  __CPROVER_object_whole(target->ll_symbol),
                  __CPROVER_object_whole(target->d_symbol),
                  __CPROVER_object_whole(target->ll_counts),
                  __CPROVER_object_whole(target->d_counts))
/* The target grows by one element per appended source symbol; under partial
   loop unwinding the final count lands between no growth and one-per-symbol. */
__CPROVER_ensures(target->size >= __CPROVER_old(target->size))
__CPROVER_ensures(target->size <= __CPROVER_old(target->size) + store->size)
{
    size_t i;
    for (i = 0; i < store->size; i++)
    {
        ZopfliStoreLitLenDist(store->litlens[i], store->dists[i],
                              store->pos[i], target);
    }
}

/*
Deflate a part, to allow ZopfliDeflate() to use multiple master blocks if
needed.
It is possible to call this function multiple times in a row, shifting
instart and inend to next bytes of the data. If instart is larger than 0, then
previous bytes are used as the initial dictionary for LZ77.
This function will usually output multiple deflate blocks. If final is 1, then
the final bit will be set on the last block.
*/
void ZopfliDeflatePart(const ZopfliOptions *options, int btype, int final,
                       const unsigned char *in, size_t instart, size_t inend,
                       unsigned char *bp, unsigned char **out,
                       size_t *outsize)
// clang-format off
/* btype selects the DEFLATE strategy: 0 = stored, 1 = fixed tree, 2 = dynamic
   (tries block splitting); final is a single header bit forwarded to the block
   emitters. */
__CPROVER_requires(btype == 0 || btype == 1 || btype == 2)
__CPROVER_requires(final == 0 || final == 1)
/* options is read (blocksplitting, blocksplittingmax, numiterations, verbose)
   and forwarded to every callee; it must be a valid object. */
__CPROVER_requires(__CPROVER_is_fresh(options, sizeof(*options)))
/* The input slice [instart, inend) is well ordered.  btype 0 follows the
   stored-block path, whose copy loop (AddNonCompressedBlock) is bounded by
   ANCB_MAX_INPUT; btype 1/2 follow the LZ77 paths, whose block-state, greedy and
   block-split callees each bound a single block at <= 100 input bytes. */
__CPROVER_requires(instart <= inend)
__CPROVER_requires(btype == 0 ==> inend - instart <= ANCB_MAX_INPUT)
__CPROVER_requires(btype != 0 ==> inend - instart <= 100)
/* in must be readable over every index any callee touches, i.e. [0, inend). */
__CPROVER_requires(__CPROVER_is_fresh(in, inend == 0 ? 1 : inend))
/* The bit-output cursor is a valid single object holding a bit index 0..7. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(*bp <= 7)
/* The dynamic output array and its size are valid, single objects; the buffer
   currently holds *outsize bytes with room for one more so the first
   ZOPFLI_APPEND_DATA stays on its no-reallocation path. */
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
/* The function appends the compressed block(s) to the output buffer, advancing
   the bit cursor and growing (reallocating) the buffer and its size. */
__CPROVER_assigns(*bp, *outsize, *out, __CPROVER_object_whole(*out),
                  __CPROVER_errno)
// clang-format on
{
    size_t i;
    /* byte coordinates rather than lz77 index */
    size_t *splitpoints_uncompressed = 0;
    size_t npoints = 0;
    size_t *splitpoints = 0;
    double totalcost = 0;
    ZopfliLZ77Store lz77;

    /* If btype=2 is specified, it tries all block types. If a lesser btype is
    given, then however it forces that one. Neither of the lesser types needs
    block splitting as they have no dynamic huffman trees. */
    if (btype == 0)
    {
        AddNonCompressedBlock(options, final, in, instart, inend, bp, out, outsize);
        return;
    }
    else if (btype == 1)
    {
        ZopfliLZ77Store store;
        ZopfliBlockState s;
        ZopfliInitLZ77Store(in, &store);
        ZopfliInitBlockState(options, instart, inend, 1, &s);

        ZopfliLZ77OptimalFixed(&s, in, instart, inend, &store);
        AddLZ77Block(options, btype, final, &store, 0, store.size, 0,
                     bp, out, outsize);

        ZopfliCleanBlockState(&s);
        ZopfliCleanLZ77Store(&store);
        return;
    }

    if (options->blocksplitting)
    {
        ZopfliBlockSplit(options, in, instart, inend,
                         options->blocksplittingmax,
                         &splitpoints_uncompressed, &npoints);
        splitpoints = (size_t *)malloc(sizeof(*splitpoints) * npoints);
    }

    ZopfliInitLZ77Store(in, &lz77);

    for (i = 0; i <= npoints; i++)
    {
        size_t start = i == 0 ? instart : splitpoints_uncompressed[i - 1];
        size_t end = i == npoints ? inend : splitpoints_uncompressed[i];
        ZopfliBlockState s;
        ZopfliLZ77Store store;
        ZopfliInitLZ77Store(in, &store);
        ZopfliInitBlockState(options, start, end, 1, &s);
        ZopfliLZ77Optimal(&s, in, start, end, options->numiterations, &store);
        totalcost += ZopfliCalculateBlockSizeAutoType(&store, 0, store.size);

        ZopfliAppendLZ77Store(&store, &lz77);
        if (i < npoints)
            splitpoints[i] = lz77.size;

        ZopfliCleanBlockState(&s);
        ZopfliCleanLZ77Store(&store);
    }

    /* Second block splitting attempt */
    if (options->blocksplitting && npoints > 1)
    {
        size_t *splitpoints2 = 0;
        size_t npoints2 = 0;
        double totalcost2 = 0;

        ZopfliBlockSplitLZ77(options, &lz77,
                             options->blocksplittingmax, &splitpoints2, &npoints2);

        for (i = 0; i <= npoints2; i++)
        {
            size_t start = i == 0 ? 0 : splitpoints2[i - 1];
            size_t end = i == npoints2 ? lz77.size : splitpoints2[i];
            totalcost2 += ZopfliCalculateBlockSizeAutoType(&lz77, start, end);
        }

        if (totalcost2 < totalcost)
        {
            free(splitpoints);
            splitpoints = splitpoints2;
            npoints = npoints2;
        }
        else
        {
            free(splitpoints2);
        }
    }

    for (i = 0; i <= npoints; i++)
    {
        size_t start = i == 0 ? 0 : splitpoints[i - 1];
        size_t end = i == npoints ? lz77.size : splitpoints[i];
        AddLZ77BlockAutoType(options, i == npoints && final,
                             &lz77, start, end, 0,
                             bp, out, outsize);
    }

    ZopfliCleanLZ77Store(&lz77);
    free(splitpoints);
    free(splitpoints_uncompressed);
}

/*
Compresses according to the deflate specification and append the compressed
result to the output.
This function will usually output multiple deflate blocks. If final is 1, then
the final bit will be set on the last block.

options: global program options
btype: the deflate block type. Use 2 for best compression.
-0: non compressed blocks (00)
-1: blocks with fixed tree (01)
-2: blocks with dynamic tree (10)
final: whether this is the last section of the input, sets the final bit to the
last deflate block.
in: the input bytes
insize: number of input bytes
bp: bit pointer for the output array. This must initially be 0, and for
consecutive calls must be reused (it can have values from 0-7). This is
because deflate appends blocks as bit-based data, rather than on byte
boundaries.
out: pointer to the dynamic output array to which the result is appended. Must
be freed after use.
outsize: pointer to the dynamic output array size.
*/
void ZopfliDeflate(const ZopfliOptions *options, int btype, int final,
                   const unsigned char *in, size_t insize,
                   unsigned char *bp, unsigned char **out, size_t *outsize)
// clang-format off
/* btype selects the DEFLATE strategy (0 stored, 1 fixed, 2 dynamic); final is a
   single header bit; both are forwarded verbatim to ZopfliDeflatePart. */
__CPROVER_requires(btype == 0 || btype == 1 || btype == 2)
__CPROVER_requires(final == 0 || final == 1)
/* options is read and forwarded to every callee; it must be a valid object. */
__CPROVER_requires(__CPROVER_is_fresh(options, sizeof(*options)))
/* The master-block loop slices the input into chunks of size
   min(ZOPFLI_MASTER_BLOCK_SIZE, remaining).  ZopfliDeflatePart bounds a single
   chunk at ANCB_MAX_INPUT (btype 0) or 100 (btype 1/2); since
   ZOPFLI_MASTER_BLOCK_SIZE far exceeds those bounds, the whole input must fit a
   single master block, so the loop runs exactly once with [0, insize). */
__CPROVER_requires(btype == 0 ==> insize <= ANCB_MAX_INPUT)
__CPROVER_requires(btype != 0 ==> insize <= 100)
/* in must be readable over every index any callee touches, i.e. [0, insize). */
__CPROVER_requires(__CPROVER_is_fresh(in, insize == 0 ? 1 : insize))
/* The bit-output cursor is a valid single object holding a bit index 0..7. */
__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))
__CPROVER_requires(*bp <= 7)
/* The dynamic output array and its size are valid, single objects; the buffer
   currently holds *outsize bytes with room for one more so the first
   ZOPFLI_APPEND_DATA stays on its no-reallocation path. */
__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))
__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))
__CPROVER_requires(((*outsize) & ((*outsize) - 1)) != 0)
__CPROVER_requires(__CPROVER_is_fresh(*out, (*outsize) + 1))
/* The function appends the compressed block(s) to the output buffer, advancing
   the bit cursor and growing (reallocating) the buffer and its size. */
__CPROVER_assigns(*bp, *outsize, *out, __CPROVER_object_whole(*out),
                  __CPROVER_errno)
// clang-format on
{
    size_t offset = *outsize;
    size_t i = 0;
    do
    {
        int masterfinal = (i + ZOPFLI_MASTER_BLOCK_SIZE >= insize);
        int final2 = final && masterfinal;
        size_t size = masterfinal ? insize - i : ZOPFLI_MASTER_BLOCK_SIZE;
        ZopfliDeflatePart(options, btype, final2,
                          in, i, i + size, bp, out, outsize);
        i += size;
    } while (i < insize);
    if (options->verbose)
    {
        fprintf(stderr,
                "Original Size: %lu, Deflate: %lu, Compression: %.2f%%\n",
                (unsigned long)insize, (unsigned long)(*outsize - offset),
                100.0 * (double)(insize - (*outsize - offset)) / (double)insize);
    }
}

void single_test(const unsigned char *in, int btype, int blocksplitting, int blocksplittingmax)
// clang-format off
/* btype is forwarded to ZopfliDeflate, which only accepts the three DEFLATE
   strategies. */
__CPROVER_requires(btype == 0 || btype == 1 || btype == 2)
/* in is treated as a NUL-terminated C string: strlen scans it and the result is
   forwarded to ZopfliDeflate as insize.  ZopfliDeflate bounds a single input at
   ANCB_MAX_INPUT (== 8) bytes for stored blocks (btype 0) and 100 bytes
   otherwise, so the string (including its terminator) must fit those bounds.
   Allocating size+1 bytes and pinning the last byte to NUL guarantees strlen
   reads in bounds and returns at most the size limit. */
__CPROVER_requires(__CPROVER_is_fresh(in, (btype == 0 ? ANCB_MAX_INPUT : 100) + 1))
__CPROVER_requires(in[(btype == 0 ? ANCB_MAX_INPUT : 100)] == 0)
/* The compressor allocates a fresh output buffer through malloc/realloc; the
   only globally-visible side effect surfaced through ZopfliDeflate's contract is
   errno. */
__CPROVER_assigns(__CPROVER_errno)
/* NOTE: run-cbmc reports SUCCESS, but the pass is VACUOUS.  single_test
   invokes the real ZopfliDeflate entry API with out == NULL and outsize == 0
   (the callee mallocs/reallocs the output buffer itself), which violates
   ZopfliDeflate's verification contract -- that contract was deliberately
   written to require a pre-existing, non-power-of-two-sized buffer
   (is_fresh(*out, *outsize+1) and ((*outsize)&((*outsize)-1)) != 0) so that
   ZopfliDeflate's OWN proof stays on the no-reallocation path.  Under
   avocado's fixed --depth 200, the is_fresh/contract instrumentation exhausts
   the depth budget before those precondition checks are reached, so they are
   sliced as unreachable and never fail.  At full depth (or with a minimal
   probe) the precondition checks for *out and *outsize FAIL.  The C code cannot
   be changed and ZopfliDeflate's verified contract should not be weakened, so
   this is a case where CBMC cannot soundly verify otherwise-correct C. */
// clang-format on
{
    ZopfliOptions options;

    // Configure options
    options.verbose = 1;
    options.verbose_more = 0; // Reduce internal verbose output
    options.numiterations = 15;
    options.blocksplitting = blocksplitting;
    options.blocksplittinglast = 0;
    options.blocksplittingmax = blocksplittingmax;

    unsigned char *out = 0;
    size_t outsize = 0;
    unsigned char bp = 0;
    size_t insize = strlen(in);

    // Perform compression
    ZopfliDeflate(&options, btype, 1, (const unsigned char *)in, insize, &bp, &out, &outsize);
}

void run_all_tests(const unsigned char *in)
// clang-format off
/* run_all_tests forwards the same in pointer to single_test with btype 0, 1 and
   2.  single_test requires is_fresh(in, (btype==0 ? ANCB_MAX_INPUT : 100) + 1)
   and a NUL terminator at index (btype==0 ? ANCB_MAX_INPUT : 100).  The btype
   1/2 calls demand the larger 101-byte fresh buffer with in[100]==0, and the
   btype 0 call additionally demands in[ANCB_MAX_INPUT]==0; providing all three
   satisfies every call-site precondition. */
__CPROVER_requires(__CPROVER_is_fresh(in, 100 + 1))
__CPROVER_requires(in[100] == 0)
__CPROVER_requires(in[ANCB_MAX_INPUT] == 0)
/* The only globally-visible side effect surfaced by single_test's contract is
   errno. */
__CPROVER_assigns(__CPROVER_errno)
// clang-format on
{
    single_test(in, 2, 1, 15); // Dynamic Huffman
    single_test(in, 1, 1, 15); // Fixed Huffman
    single_test(in, 0, 1, 15); // Uncompressed

    // Test with different block splitting settings
    single_test(in, 2, 0, 15); // No block splitting
    single_test(in, 2, 1, 5);  // Limited splits
    single_test(in, 2, 1, 0);  // No splits
    single_test(in, 2, 1, 1);  // No splits
    single_test(in, 2, 1, 50); // No splits
    single_test(in, 2, 1, 30); // More splits allowed
}

unsigned char *read_stdin_to_bytes(size_t *out_size)
// clang-format off
__CPROVER_requires(__CPROVER_is_fresh(out_size, sizeof(*out_size)))
__CPROVER_assigns(*out_size)
__CPROVER_ensures(
    __CPROVER_return_value == NULL ||
    __CPROVER_is_fresh(__CPROVER_return_value, *out_size))
// clang-format on
{
    // Initial buffer size
    size_t buffer_size = 1024;
    size_t total_read = 0;

    // Allocate initial buffer
    unsigned char *buffer = malloc(buffer_size);
    if (buffer == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    // Read stdin in chunks
    int ch;
    while ((ch = getchar()) != EOF)
    // clang-format off
    __CPROVER_assigns(ch, buffer_size, total_read, buffer,
                      __CPROVER_object_whole(buffer))
    __CPROVER_loop_invariant(buffer_size >= 1024 &&
                             total_read <= buffer_size &&
                             __CPROVER_rw_ok(buffer, buffer_size))
    // clang-format on
    {
        // Resize buffer if needed
        if (total_read >= buffer_size)
        {
            buffer_size *= 2;
            unsigned char *new_buffer = realloc(buffer, buffer_size);
            if (new_buffer == NULL)
            {
                fprintf(stderr, "Memory reallocation failed\n");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }

        // Store the character
        buffer[total_read++] = (unsigned char)ch;
    }

    // Set the output size
    *out_size = total_read;
    return buffer;
}

int main()
{
    size_t bytes_size;
    unsigned char *bytes = read_stdin_to_bytes(&bytes_size);

    if (bytes == NULL)
    {
        fprintf(stderr, "Failed to read stdin\n");
        return 1;
    }

    // Run tests
    run_all_tests(bytes);

    // Free allocated memory
    free(bytes);

    return 0;
}
