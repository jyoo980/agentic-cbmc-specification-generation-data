/* CBMC model for ioctl(), the only external callee of getWindowSize that has no
 * library body in CPROVER (write/snprintf/strlen are modeled, getCursorPosition
 * is replaced by its contract).  Without a body CBMC raises a "no body for
 * callee ioctl" failure, so this stub supplies a sound non-deterministic one: it
 * returns an arbitrary status and leaves the caller's winsize untouched (so its
 * fields stay fully unconstrained).  We deliberately do NOT touch the variadic
 * winsize argument: reaching into the va_list would add its own (spurious)
 * pointer-validity proof obligations, and leaving the struct nondeterministic is
 * already the soundest model of "ioctl may report any geometry".  Linked only
 * when ioctl is an external callee (i.e. for getWindowSize). */
int __VERIFIER_nondet_int(void);

/* FUNCTION: ioctl */
int ioctl(int fd, unsigned long request, ...) {
    (void)fd; (void)request;
    return __VERIFIER_nondet_int();
}
