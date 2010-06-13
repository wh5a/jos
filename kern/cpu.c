/*
 * CPU setup and management of key protected-mode data structures,
 * such as global descriptor table (GDT) and task state segment (TSS).
 *
 * Copyright (C) 2010 Yale University.
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Primary author: Bryan Ford
 */

#include <inc/assert.h>
#include <inc/string.h>

#include <inc/memlayout.h>
#include <kern/cpu.h>
#include <kern/init.h>

cpu cpu_boot = {

	// Global descriptor table for bootstrap CPU.
	// The GDTs for other CPUs are copied from this and fixed up.
	//
	// The kernel and user segments are identical except for the DPL.
	// To load the SS register, the CPL must equal the DPL.  Thus,
	// we must duplicate the segments for the user and the kernel.
	//
	// The only descriptor that differs across CPUs is the TSS descriptor.
	//
	gdt: {
	// 0x0 - unused (always faults -- for trapping NULL far pointers)
	SEG_NULL,

	// 0x8 - kernel code segment
	[GD_KT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 0),

	// 0x10 - kernel data segment
	[GD_KD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 0),

	// 0x18 - user code segment
	[GD_UT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 3),

	// 0x20 - user data segment
	[GD_UD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 3),

	// 0x28 - tss, initialized in idt_init()
	[GD_TSS >> 3] = SEG_NULL
	},

	magic: CPU_MAGIC
};

