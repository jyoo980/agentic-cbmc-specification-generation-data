/* CBMC verification stubs.
   This CBMC build leaves malloc/free as symex intrinsics with no goto-program
   body, which makes the DFCC contracts library fail to instrument ("no body
   for function 'free' / '__CPROVER_deallocate' during inlining").
   We supply minimal models so contract instrumentation can proceed.
   malloc still allocates a real fresh object via __CPROVER_allocate, so bounds
   and pointer checks remain sound; deallocation is modeled as a no-op (only
   affects leak/use-after-free tracking, not the functional contracts). */
#include <stddef.h>

extern void *__CPROVER_allocate(__CPROVER_size_t size, __CPROVER_bool zero);

void __CPROVER_deallocate(void *ptr) { (void)ptr; }

void *malloc(size_t size) { return __CPROVER_allocate(size, 0); }

void free(void *ptr) { __CPROVER_deallocate(ptr); }
