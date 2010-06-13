/*
 * Kernel debugging support.
 *
 * Copyright (C) 2010 Yale University.
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Primary author: Bryan Ford
 */

#ifndef PIOS_KERN_DEBUG_H_
#define PIOS_KERN_DEBUG_H_
#ifndef JOS_KERNEL
# error "This is a kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <inc/gcc.h>


#define DEBUG_TRACEFRAMES	10


void debug_warn(const char*, int, const char*, ...);
void debug_panic(const char*, int, const char*, ...) gcc_noreturn;
void debug_trace(uint32_t ebp, uint32_t eips[DEBUG_TRACEFRAMES]);
void debug_check(void);

#endif /* PIOS_KERN_DEBUG_H_ */
