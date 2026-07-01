#include <stddef.h>
#include <stdio.h>

/* FUNCTION: snprintf */

/* A nondeterministic model of C `snprintf`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models; for the variadic `snprintf` that path drags
 * in the variadic-aware library handling that crashes
 * `goto-instrument --enforce-contract` (the same class of failure that makes the
 * variadic `editorSetStatusMessage` callers unverifiable).
 *
 * `snprintf` formats its arguments into the buffer `str`, writing at most `size`
 * bytes (including the terminating NUL) and returning the number of bytes that
 * would have been written had `size` been unbounded.  Callers in this file use
 * the formatted bytes only as the source of a subsequent `write`, never
 * inspecting their contents, so the model ignores `format`/varargs.  It
 * NUL-terminates `str` when `size > 0` (keeping the buffer a valid C string) and
 * returns a nondeterministic non-negative `result`. */
int snprintf(char *str, size_t size, const char *format, ...)
{
    (void)format;

    if (size > 0) str[0] = '\0';

    int result;
    __CPROVER_assume(result >= 0);
    return result;
}
