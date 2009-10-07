// idle loop

#include <inc/x86.h>
#include <inc/lib.h>

void
umain(void)
{
	binaryname = "idle";

	// Loop forever, simply trying to yield to a different environment.
	// Instead of busy-waiting like this,
	// a better way would be to use the processor's HLT instruction
	// to cause the processor to stop executing until the next interrupt -
	// doing so allows the processor to conserve power more effectively.
	while (1) {
		sys_yield();

		// Break into the JOS kernel monitor after each sys_yield().
		// A real, "production" OS of course would NOT do this -
		// it would just endlessly loop waiting for hardware interrupts
		// to cause other environments to become runnable.
		// However, in JOS it is easier for testing and grading
		// if we invoke the kernel monitor after each iteration,
		// because the first invocation of the idle environment
		// usually means everything else has run to completion.
		breakpoint();
	}
}

