// User-level page fault handler support.
// We use an assembly language wrapper around a C function.
// The assembly wrapper is in pfentry.S.

#include <inc/lib.h>


// Assembly language pgfault entrypoint defined in lib/pgfaultentry.S.
extern void _pgfault_upcall(void);

// Pointer to currently installed C-language pgfault handler.
void (*_pgfault_handler)(struct UTrapframe *utf);

//
// Set the page fault handler function.
// If there isn't one yet, _pgfault_handler will be 0.
//
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_pgfault_handler == 0) {
// The first time we register a handler, we need to 
// allocate an exception stack (one page of memory with its top
// at UXSTACKTOP), and tell the kernel to call the assembly-language
// _pgfault_upcall routine when a page fault occurs.
          int ret;
          ret = sys_page_alloc(0, (void *)UXSTACKTOP - PGSIZE, PTE_W | PTE_U | PTE_P);
          if (ret)
            panic("sys_page_alloc: %e", ret);
          sys_env_set_pgfault_upcall(0, _pgfault_upcall);
	}

	// Save handler pointer for assembly to call.
	_pgfault_handler = handler;
}

