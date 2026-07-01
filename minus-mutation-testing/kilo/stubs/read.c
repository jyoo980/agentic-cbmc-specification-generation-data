/* CBMC stub model for read.
 *
 * goto-cc automatically links a CPROVER library model of read() that is built
 * around the internal `__CPROVER_pipes` machinery. That model carries its own
 * internal assertions about the destination buffer (see the
 * `read.array_bounds.*` / `read.pointer_dereference.*` properties it emits);
 * those internal obligations fail for ordinary callers that pass a small
 * stack buffer, and injecting the model via `goto-instrument --add-library`
 * makes the assigns-clause instrumentation abort outright with an invariant
 * violation (`<builtin-library-read>` ... `read::1::1::error`).
 *
 * This stub supplies a sound, self-contained model: read() may fill up to
 * `nbyte` bytes of the destination buffer with arbitrary input, and returns
 * either -1 (error) or the number of bytes read (0..nbyte). The destination is
 * required to be writable for `nbyte` bytes, which is exactly the memory-safety
 * obligation a real read() imposes on its caller. Havocking the whole buffer is
 * a sound over-approximation of "copy in the bytes that were read".
 */

/* FUNCTION: read */

#include <sys/types.h>

ssize_t read(int fildes, void *buf, size_t nbyte) {
    (void)fildes;

    if (nbyte == 0) {
        return 0;
    }

    /* The caller must provide a buffer of at least nbyte writable bytes. */
    __CPROVER_precondition(__CPROVER_w_ok(buf, nbyte),
                           "read: buffer must point to nbyte writable bytes");

    /* read() fills the destination with arbitrary input bytes; modelling this
       as a full havoc of the nbyte-byte region is a sound over-approximation. */
    for (size_t i = 0; i < nbyte; i++) {
        char nondet_byte;
        ((char *)buf)[i] = nondet_byte;
    }

    /* On success read returns the number of bytes read (0..nbyte); -1 on error. */
    ssize_t result;
    __CPROVER_assume(result >= -1 && result <= (ssize_t)nbyte);
    return result;
}
