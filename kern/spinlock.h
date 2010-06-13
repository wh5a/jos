/*
 * Spin locks for multiprocessor mutual exclusion in the kernel.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the xv6 instructional operating system from MIT.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#ifndef PIOS_KERN_SPINLOCK_H
#define PIOS_KERN_SPINLOCK_H
#ifndef JOS_KERNEL
# error "This is a kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <kern/debug.h>

// Mutual exclusion lock.
typedef struct spinlock {
	uint32_t locked;	// Is the lock held?

	// For debugging:
	const char *file;	// Source file where spinlock_init() was called
	int line;		// Line number of spinlock_init()
	struct cpu *cpu;	// The cpu holding the lock.
	uint32_t eips[DEBUG_TRACEFRAMES]; // Call stack that locked the lock.
} spinlock;

#define spinlock_init(lk)	spinlock_init_(lk, __FILE__, __LINE__)


void spinlock_init_(spinlock *lk, const char *file, int line);
void spinlock_acquire(spinlock *lk);
void spinlock_release(spinlock *lk);
int spinlock_holding(spinlock *lk);
void spinlock_check();

#endif /* !PIOS_KERN_SPINLOCK_H */
