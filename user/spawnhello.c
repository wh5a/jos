#include <inc/lib.h>

void
umain(void)
{
	int r;
	cprintf("i am parent environment %08x\n", env->env_id);
	if ((r = spawnl("hello", "hello", 0)) < 0)
		panic("spawn(hello) failed: %e", r);
}
