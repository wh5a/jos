#include <kern/time.h>
#include <inc/assert.h>

static unsigned int ticks;

void
time_init(void) 
{
	ticks = 0;
}

// this is called once per timer interupt; a timer interupt fires 100 times a
// second
void
time_tick(void) 
{
	ticks++;
	if (ticks * 10 < ticks)
		panic("time_tick: time overflowed");
}

unsigned int
time_msec(void) 
{
	return ticks * 10;
}
