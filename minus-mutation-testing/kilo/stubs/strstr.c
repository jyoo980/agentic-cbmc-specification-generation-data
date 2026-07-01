/* CBMC stub model for strstr.
 *
 * goto-cc/goto-instrument do not supply a bundled library model for strstr
 * (CBMC reports `no body for callee strstr`). Without a body the call havocs
 * memory, which corrupts the global pattern arrays a caller iterates over and
 * produces spurious out-of-bounds failures.
 *
 * This stub is a faithful, self-contained implementation of the C standard
 * library semantics: it returns a pointer to the first occurrence of `needle`
 * in `haystack`, or NULL if `needle` is not present. An empty needle matches at
 * the start of `haystack`. Both arguments must be valid NUL-terminated strings,
 * exactly the memory-safety obligation the real strstr imposes on its caller.
 *
 * Because the returned pointer provably points into `haystack` at a position
 * where all `strlen(needle)` characters of the match lie inside the string,
 * callers that read one byte past the match (e.g. `p[strlen(needle)]`) remain
 * in bounds.
 */

/* FUNCTION: strstr */

#include <stddef.h>

char *strstr(const char *haystack, const char *needle) {
    /* An empty needle matches at the beginning of the haystack. */
    if (needle[0] == '\0') {
        return (char *)haystack;
    }

    for (const char *h = haystack; *h != '\0'; h++) {
        const char *hp = h;
        const char *np = needle;
        while (*np != '\0' && *hp == *np) {
            hp++;
            np++;
        }
        if (*np == '\0') {
            /* Full needle matched starting at h. */
            return (char *)h;
        }
    }

    return NULL;
}
