/*
 * Kernel initialization.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#ifndef PIOS_KERN_INIT_H
#define PIOS_KERN_INIT_H
#ifndef JOS_KERNEL
# error "This is a kernel header; user programs should not #include it"
#endif

#include <inc/gcc.h>


// Called on each processor to initialize the kernel.
void init(void);

// First function run in user mode (only on one processor)
void user(void);

// Called when there is no more work left to do in the system.
// The grading scripts trap calls to this to know when to stop.
void done(void) gcc_noreturn;


#endif /* !PIOS_KERN_INIT_H */
