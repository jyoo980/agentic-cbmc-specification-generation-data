/* CBMC model for signal().  Installing a signal handler has no effect that any
 * contract in this codebase observes, so this is a side-effect-free no-op that
 * returns the just-passed handler as the "previous" one.  glibc compiled with
 * _DEFAULT_SOURCE redirects the signal identifier to __sysv_signal, so that is
 * the symbol whose body is actually missing; the FUNCTION marker below is what
 * maps the signal external callee to this file.  Linked only when signal is an
 * external callee (i.e. initEditor / handleSigWinCh), so no other verification
 * is affected. */
#include <signal.h>

/* FUNCTION: signal */
void (*__sysv_signal(int sig, void (*handler)(int)))(int)
{
    (void)sig;
    return handler;
}
