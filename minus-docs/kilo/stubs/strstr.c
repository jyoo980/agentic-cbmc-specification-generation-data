#include <string.h>

/* FUNCTION: strstr */

/* A precise model of `strstr`.  CBMC ships no body for `strstr`, so without this
 * stub the call is a missing-body callee returning an unconstrained pointer that
 * the caller then dereferences (`p[patlen]` in `editorSelectSyntaxHighlight`),
 * producing spurious pointer failures.
 *
 * This is the exact textbook semantics: scan `haystack` for the first position
 * where `needle` occurs as a substring, returning a pointer to it, or NULL if it
 * never occurs (the empty needle matches at position 0).  Unlike a
 * nondeterministic over-approximation, the precise version lets a caller prove a
 * *definite* match result (used by `editorSelectSyntaxHighlight`'s contract,
 * which pins E.syntax to the matched entry).
 *
 * Memory safety: both arguments are NUL-terminated, and the inner comparison
 * stops as soon as `needle[k]` reaches the terminator or a byte differs, so the
 * deepest read into `haystack` is its own terminator — always in bounds. */
char *strstr(const char *haystack, const char *needle)
{
    for (size_t i = 0; ; i++) {
        size_t k = 0;
        while (needle[k] != '\0' && haystack[i + k] == needle[k])
            k++;
        if (needle[k] == '\0')
            return (char *)haystack + i;
        if (haystack[i] == '\0')
            return (char *)0;
    }
}
