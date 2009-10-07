#include <inc/lib.h>

void
umain(void)
{
	int r;
	cprintf("i am parent environment %08x\n", env->env_id);
	if ((r = spawnl("init", "init", "one", "two", 0)) < 0)
		panic("spawnl(init) failed: %e", r);
}
