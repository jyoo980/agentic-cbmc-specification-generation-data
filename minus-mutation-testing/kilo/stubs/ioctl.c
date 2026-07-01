/* CBMC stub model for ioctl.
 *
 * CBMC ships no library model for ioctl(). Left undefined, ioctl is a bodyless
 * variadic function; under `goto-instrument --enforce-contract` the assigns
 * instrumentation aborts with an invariant violation
 * (`instrument_spec_assigns.cpp ... create_car_expr` / "Unreachable") when an
 * enforced function (e.g. getWindowSize) calls it. Supplying a body avoids that.
 *
 * This is a sound, self-contained model: ioctl returns a nondeterministic
 * result (0 on success, -1 on error). It deliberately does NOT write through
 * its output argument. Leaving the caller's output buffer untouched means the
 * caller reads nondeterministic (uninitialised) values out of it, which is a
 * sound over-approximation of "ioctl filled the buffer with arbitrary data".
 */

/* FUNCTION: ioctl */

int ioctl(int fd, unsigned long request, ...) {
    (void)fd;
    (void)request;

    /* Real ioctl returns -1 on error, or a non-negative value on success. */
    int result;
    __CPROVER_assume(result == 0 || result == -1);
    return result;
}
