/*
 * Kernel debugging support.
 * Called throughout the kernel, especially by assert() macro.
 *
 * Copyright (C) 2010 Yale University.
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Primary author: Bryan Ford
 */

#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/debug.h>
#include <kern/spinlock.h>


// Variable panicstr contains argument to first call to panic; used as flag
// to indicate that the kernel has already called panic and avoid recursion.
static const char *panicstr;

// Panic is called on unresolvable fatal errors.
// It prints "panic: mesg", and then enters the kernel monitor.
void
debug_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;
	int i;

	// Avoid infinite recursion if we're panicking from kernel mode.
	if ((read_cs() & 3) == 0) {
		if (panicstr)
			goto dead;
		panicstr = fmt;
	}

	// First print the requested message
	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

	// Then print a backtrace of the kernel call chain
	uint32_t eips[DEBUG_TRACEFRAMES];
	debug_trace(read_ebp(), eips);
	for (i = 0; i < DEBUG_TRACEFRAMES && eips[i] != 0; i++)
		cprintf("  from %08x\n", eips[i]);

dead:
	while (1) ;	// just spin
}

/* like panic, but don't */
void
debug_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}

// Record the current call stack in eips[] by following the %ebp chain.
void gcc_noinline
debug_trace(uint32_t ebp, uint32_t eips[DEBUG_TRACEFRAMES])
{
	const uint32_t *frame = (const uint32_t*)ebp;
	int i;

	for (i = 0; i < 10 && frame; i++) {
		eips[i] = frame[1];		// saved %eip
		frame = (uint32_t*)frame[0];	// saved ebp
	}
	for (; i < 10; i++)	// zero out rest of eips
		eips[i] = 0;
}


static void gcc_noinline f3(int r, uint32_t *e) { debug_trace(read_ebp(), e); }
static void gcc_noinline f2(int r, uint32_t *e) { r & 2 ? f3(r,e) : f3(r,e); }
static void gcc_noinline f1(int r, uint32_t *e) { r & 1 ? f2(r,e) : f2(r,e); }

// Test the backtrace implementation for correct operation
void
debug_check(void)
{
	uint32_t eips[4][DEBUG_TRACEFRAMES];
	int r, i;

	// produce several related backtraces...
	for (i = 0; i < 4; i++)
		f1(i, eips[i]);

	// ...and make sure they come out correctly.
	for (r = 0; r < 4; r++)
		for (i = 0; i < DEBUG_TRACEFRAMES; i++) {
			assert((eips[r][i] != 0) == (i < 5));
			if (i >= 2)
				assert(eips[r][i] == eips[0][i]);
		}
	assert(eips[0][0] == eips[1][0]);
	assert(eips[2][0] == eips[3][0]);
	assert(eips[1][0] != eips[2][0]);
	assert(eips[0][1] == eips[2][1]);
	assert(eips[1][1] == eips[3][1]);
	assert(eips[0][1] != eips[1][1]);

	cprintf("debug_check() succeeded!\n");
}

