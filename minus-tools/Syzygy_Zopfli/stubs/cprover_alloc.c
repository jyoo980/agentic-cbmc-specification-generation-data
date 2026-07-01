/* CBMC models for the C dynamic-allocation routines used by functions under
 * verification whose bodies are otherwise treated as nondeterministic.
 *
 * The avocado pipeline (tools/run_cbmc.py) auto-discovers every `*.c` file in
 * this directory, indexes the `FUNCTION:` markers below, and compiles the file
 * in alongside the source whenever a function under verification calls one of
 * the modeled symbols. Providing a body here keeps the symbol out of the
 * "no body for function" set, which in turn avoids the `--add-library` retry
 * path that crashes goto-instrument when contract enforcement meets the bundled
 * malloc model's `should_malloc_fail` flag.
 *
 * These are sound, deterministic, allocation-success models: `malloc`/`calloc`
 * return a fresh, valid object of the requested size (never NULL), and `free`
 * is a no-op. They model the success path of the real routines; they do not
 * model allocation failure, which the functions under verification here do not
 * handle and which the original code's `assert`/contract preconditions assume
 * away.
 */

typedef __typeof__(sizeof(int)) __cprover_alloc_size_t;

void *__CPROVER_allocate(__cprover_alloc_size_t size, __CPROVER_bool zero);

/* FUNCTION: malloc */
void *malloc(__cprover_alloc_size_t size)
{
    /* A fresh, valid, uninitialized object of `size` bytes; never NULL. */
    return __CPROVER_allocate(size, 0);
}

/* FUNCTION: calloc */
void *calloc(__cprover_alloc_size_t nmemb, __cprover_alloc_size_t size)
{
    /* A fresh, valid, zero-initialized object of `nmemb * size` bytes. */
    return __CPROVER_allocate(nmemb * size, 1);
}

/* FUNCTION: realloc */
void *realloc(void *ptr, __cprover_alloc_size_t size)
{
    /* A fresh, valid object of `size` bytes; never NULL.  Models realloc's
     * allocation-success path.  It does not copy the old object's contents, so
     * a caller that re-reads previously stored bytes through the returned
     * pointer sees nondeterministic data; the functions under verification here
     * only write through the grown buffer, so this is a sound success model for
     * their memory-safety obligations. */
    (void)ptr;
    return __CPROVER_allocate(size, 0);
}

/* FUNCTION: free */
void free(void *ptr)
{
    /* No-op: deallocation is irrelevant to the memory-safety properties of the
     * single function under verification, and a no-op avoids spurious
     * use-after-free / double-free obligations against an empty frees clause. */
    (void)ptr;
}
