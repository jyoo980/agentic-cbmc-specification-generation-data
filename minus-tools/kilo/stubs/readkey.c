/* CBMC models for editorReadKey's external callees: read() and exit().
 * read() delivers a scripted byte/return-code so the key decoder becomes a
 * deterministic function of the rk_* globals that the contract pins exactly;
 * exit() never returns.  Linked only when read()/exit() are external callees
 * (i.e. only for editorReadKey). */
#include <stddef.h>
typedef long ssize_t;
#define RK_MAX 6
int  rk_idx;
int  rk_ret[RK_MAX];
char rk_byte[RK_MAX];
/* FUNCTION: read */
ssize_t read(int fd, void *buf, size_t count) {
    (void)fd; (void)count;
    int i = rk_idx;
    rk_idx = i + 1;
    if (i >= RK_MAX) return 0;
    if (rk_ret[i] > 0) ((char *)buf)[0] = rk_byte[i];
    return (ssize_t)rk_ret[i];
}
/* FUNCTION: exit */
void exit(int status) { (void)status; __CPROVER_assume(0); }
