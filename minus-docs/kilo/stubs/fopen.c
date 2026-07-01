#include <stdio.h>

/* FUNCTION: fopen */
/* FUNCTION: fopen64 */

/* A nondeterministic model of `fopen`.  On Linux with 64-bit file offsets the
 * `fopen` call in the source resolves to the `fopen64` symbol, so the body is
 * defined for `fopen64` (and the `fopen` marker is present so the stub index
 * selects this file for the `fopen` external callee).
 *
 * Providing a body avoids CBMC's built-in `fopen64` model, whose `fopen_error`
 * bool crashes `goto-instrument --enforce-contract`'s assigns-frame
 * instrumentation ("no definite size for lvalue target ... fopen64").  The
 * `--add-library` injection (forced by replacing a callee that uses
 * `__CPROVER_is_fresh`) will not overwrite a symbol that already has a body.
 *
 * The model returns either NULL (open failed) or a pointer to a freshly
 * allocated `FILE` object (open succeeded), so both branches of the caller's
 * `if (!fp)` check are explored.  The returned handle is never dereferenced by
 * the callers here (only passed to the `getline`/`fclose` stubs and NULL-tested),
 * so an uninitialized `FILE` object is sufficient. */
FILE *fopen64(const char *path, const char *mode)
{
    (void)path;
    (void)mode;

    int fail;
    if (fail)
        return (FILE *)0;

    return __CPROVER_allocate(sizeof(FILE), 0);
}
