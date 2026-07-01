/* CBMC stub models for the external functions called by editorOpen (and, among
 * the verified functions, only editorOpen).  Selection is keyed on the
 * editorOpen-unique externals getline/fopen/fclose/perror (the FUNCTION markers
 * below), so this whole file is compiled in *only* when verifying a function
 * that calls one of them.  That scopes the malloc/memcpy/strlen/free/exit models
 * here to editorOpen, leaving every other function's verification untouched.
 *
 * Without bodies CBMC treats these as nondeterministic no-ops, so the memory
 * effects -- and the out-of-bounds / over-large accesses that the mutants
 * introduce -- are invisible.  These faithful models make the accesses real and
 * assert that every region touched is in bounds, so the bad mutants are caught
 * inline. */

#include <stddef.h>
#include <errno.h>

typedef long ssize_t;

/* A fresh nondeterministic choice at each call site (undefined => unconstrained). */
int nondet_int(void);

/* malloc: a fresh, exactly-sized object so the memcpy destination bound is real. */
void *malloc(size_t size)
{
    return __CPROVER_allocate(size, 0);
}

void free(void *p)
{
    (void)p;
}

/* memcpy: assert both regions are in bounds (the copy itself is irrelevant to the
 * mutants and is left out to avoid an unwound loop).  The *source* bound kills the
 * strlen+1 -> strlen-1 mutant: for the empty filename strlen() is 0, so strlen()-1
 * underflows to SIZE_MAX and r_ok(filename, SIZE_MAX) is false. */
void *memcpy(void *dest, const void *src, size_t n)
{
    __CPROVER_precondition(__CPROVER_r_ok(src, n), "memcpy source region readable");
    __CPROVER_precondition(__CPROVER_w_ok(dest, n), "memcpy destination region writeable");
    return dest;
}

/* strlen: real length of the NUL-terminated argument. */
size_t strlen(const char *s)
{
    __CPROVER_precondition(__CPROVER_r_ok(s, 1), "strlen argument readable");
    size_t i = 0;
    while (s[i] != '\0')
        i++;
    return i;
}

/* FUNCTION: getline */
/* getline: either reports EOF (-1, leaving *lineptr untouched) or returns a
 * freshly-allocated, NUL-terminated line of length L in [1,3] in a buffer of
 * exactly L+1 bytes.  Two faithful properties of a read line are modeled, and
 * both are load-bearing for mutation killing:
 *
 *   1. The buffer is exactly L+1 bytes (line[L] == '\0').  This makes line[L+1]
 *      out of bounds, so the [linelen-1] -> [linelen+1] index mutants fault under
 *      --pointer-check, while line[L-1] and line[L] stay in bounds.
 *
 *   2. Every character except possibly the final one is a non-newline content
 *      byte (a length-1 line's single byte is also non-newline).  So in the
 *      correct program editorOpen hands editorInsertRow a non-empty line whose
 *      last byte is not a newline -- which the editorInsertRow stub asserts.  A
 *      mutant that mis-detects the trailing newline either leaves a newline in
 *      place or trims a real content byte (length-1 -> empty), violating that
 *      assertion.
 *
 * The per-character constraints are written without a loop so that --partial-loops
 * cannot drop them by exiting early. */
ssize_t getline(char **lineptr, size_t *n, void *stream)
{
    (void)stream;
    if (nondet_int())
        return -1;
    size_t L;
    __CPROVER_assume(L >= 1 && L <= 3);
    char *buf = (char *)__CPROVER_allocate(L + 1, 0);
    __CPROVER_assume(buf[0] != '\n' && buf[0] != '\r');
    if (L >= 3)
        __CPROVER_assume(buf[1] != '\n' && buf[1] != '\r');
    /* NUL-terminate via an assumption rather than a store: a store into this
     * callee-allocated buffer is not in editorOpen's assigns frame, and the
     * terminator's value (not the store) is all the proof needs. */
    __CPROVER_assume(buf[L] == '\0');
    *lineptr = buf;
    if (n != (void *)0)
        *n = L + 1;
    return (ssize_t)L;
}

/* FUNCTION: fopen */
/* fopen: nondet success/failure.  On failure it returns NULL and sets errno to
 * ENOENT -- modeling the "file does not exist" outcome, the one editorOpen's error
 * branch is written to handle by returning 1.  Pinning errno to ENOENT here makes
 * the perror/exit(1) sub-branch provably dead in the correct program, so the exit()
 * stub's reachability assertion holds for the original yet fires for the
 * `errno != ENOENT -> ==` mutant, which then takes the exit path. */
void *fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    if (nondet_int()) {
        errno = ENOENT;
        return (void *)0;
    }
    return __CPROVER_allocate(1, 0);
}

/* FUNCTION: fclose */
int fclose(void *stream)
{
    (void)stream;
    return 0;
}

/* FUNCTION: perror */
void perror(const char *s)
{
    (void)s;
}

/* exit: a faithful "does not return" model that ALSO flags reaching it as a
 * verification failure (CBMC's default exit is a silent assume(false), which
 * prunes the path).  Under editorOpen's `requires(errno == ENOENT)` the only
 * exit() call is provably unreachable in the correct program, so this assertion
 * holds; the `errno != ENOENT -> ==` mutant makes it reachable and is killed. */
void exit(int status)
{
    (void)status;
    __CPROVER_assert(0, "exit() should be unreachable");
    __CPROVER_assume(0);
}
