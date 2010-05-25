// test user fault handler being called with no exception stack mapped

#include <inc/lib.h>

void _pgfault_upcall();

void
umain(void)
{
	sys_env_set_pgfault_upcall(0, (void*) _pgfault_upcall);
	*(int*)0 = 0;
}
