/* See COPYRIGHT for copyright information. */

/* Support for two time-related hardware gadgets: 1) the run time
 * clock with its NVRAM access functions; 2) the 8253 timer, which
 * generates interrupts on IRQ 0.
 */

#include <inc/x86.h>
#include <inc/stdio.h>
#include <inc/isareg.h>
#include <inc/timerreg.h>

#include <kern/kclock.h>
#include <kern/picirq.h>


unsigned
mc146818_read(unsigned reg)
{
	outb(IO_RTC, reg);
	return inb(IO_RTC+1);
}

void
mc146818_write(unsigned reg, unsigned datum)
{
	outb(IO_RTC, reg);
	outb(IO_RTC+1, datum);
}


void
kclock_init(void)
{
	/* initialize 8253 clock to interrupt 100 times/sec */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(IO_TIMER1, TIMER_DIV(100) % 256);
	outb(IO_TIMER1, TIMER_DIV(100) / 256);
	cprintf("	Setup timer interrupts via 8259A\n");
	irq_setmask_8259A(irq_mask_8259A & ~(1<<0));
	cprintf("	unmasked timer interrupt\n");
}

