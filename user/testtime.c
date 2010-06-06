#include <inc/lib.h>

void
sleep(int sec) 
{
	unsigned end = sys_time_msec() + sec * 1000;
	while (sys_time_msec() < end)
		sys_yield();
}

void
umain(int argc, char **argv)
{
	int i;

	sleep(2);

	cprintf("starting count down: ");
	for (i = 10; i >= 0; i--) {
		cprintf("%d ", i);
		sleep(1);
	}
	cprintf("\n");
}
