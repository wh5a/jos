// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display backtrace", mon_backtrace },
	{ "showmap", "Display virtual to physical mapping", mon_showmap },
	{ "alloc_page", "Allocate a physical page", mon_alloc_page },
	{ "page_status", "Display page status", mon_page_status },
	{ "free_page", "Free an allocated page", mon_free_page },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start %08x (virt)  %08x (phys)\n", _start, _start - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-_start+1023)/1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
  uint32_t ebp = read_ebp(), *ebpp, eip, i;
  struct Eipdebuginfo dbg;
  while (ebp > 0) {
    ebpp = (uint32_t *)ebp;
    eip = ebpp[1];
    cprintf("ebp %x eip %x args", ebp, eip);
    for (i = 2; i < 6; i++) {
      cprintf(" %08x", ebpp[i]);
    }
    debuginfo_eip(eip, &dbg);
    cprintf("\n\t%s:%d: ", dbg.eip_file, dbg.eip_line);
    for (i = 0; i < dbg.eip_fn_namelen; i++)
      cputchar(dbg.eip_fn_name[i]);
    cprintf("+%d\n", eip - dbg.eip_fn_addr);
    ebp = *ebpp;
  }
  return 0;
}

int
mon_alloc_page(int argc, char **argv, struct Trapframe *tf)
{
	int err;
	struct Page *pp;

	err = page_alloc(&pp);
	if (err) {
		cprintf("Page allocation failed\n");
		return 0;
	}

	cprintf("phys: 0x%08x virt: 0x%08x\n", page2pa(pp),
		(uintptr_t) page2kva(pp));
	return 0;
}

int
mon_free_page(int argc, char **argv, struct Trapframe *tf)
{
	struct Page *pp;
	physaddr_t ph;

	if (argc != 2) {
		cprintf("Usage: free_page <page_phys_addr>\n");
		return 0;
	}

	ph = (physaddr_t) strtol(argv[1], NULL, 16);
	pp = pa2page(ph);
	page_free(pp);
	return 0;
}

static void
dump_pde_flags(pde_t *pde)
{
	cprintf(" pde: 0x%08x [", *pde);
        cprintf(" 0x%08x", PTE_ADDR(*pde));
	if (*pde & PTE_A)
		cprintf(" A");
	if (*pde & PTE_PCD)
		cprintf(" PCD");
	if (*pde & PTE_PWT)
		cprintf(" PWT");
	if (*pde & PTE_U)
		cprintf(" U");
	else
		cprintf(" S");
	if (*pde & PTE_W)
		cprintf(" W");
	else
		cprintf(" R");
	if (*pde & PTE_P)
		cprintf(" P");
	cprintf(" ]\n");
}

static void
dump_pte_flags(pte_t *pte)
{
	cprintf(" pte: 0x%08x [ 0x%08x", *pte, PTE_ADDR(*pte));
	if (*pte & PTE_D)
		cprintf(" D");
	if (*pte & PTE_A)
		cprintf(" A");
	if (*pte & PTE_PCD)
		cprintf(" PCD");
	if (*pte & PTE_PWT)
		cprintf(" PWT");
	if (*pte & PTE_U)
		cprintf(" U");
	else
		cprintf(" S");
	if (*pte & PTE_W)
		cprintf(" W");
	else
		cprintf(" R");
	if (*pte & PTE_P)
		cprintf(" P");
	cprintf(" ]\n");
}

int
mon_page_status(int argc, char **argv, struct Trapframe *tf)
{
	struct Page *pp;
	physaddr_t ph;

	if (argc != 2) {
		cprintf("Usage: page_status <page_phys_addr>\n");
		return 0;
	}

	ph = (physaddr_t) strtol(argv[1], NULL, 16);
	LIST_FOREACH(pp, &page_free_list, pp_link)
		if (page2pa(pp) == ph) {
			cprintf("free\n");
			return 0;
		}

	cprintf("allocated\n");
	return 0;
}

int
mon_showmap(int argc, char **argv, struct Trapframe *tf)
{
	int i;
	pde_t *pgdir = KADDR(rcr3());

	for (i = 1; i < argc; i++) {
		pde_t *pde, *pte;
		uintptr_t va, ph;

		va = strtol(argv[i], NULL, 16);

		pde = pgdir + PDX(va);
		if (!(*pde & PTE_P)) {
			cprintf("\n0x%08x not mapped\n", va);
			continue;
		}

		pte = 0;

                pte = (pte_t *) KADDR(PTE_ADDR(*pde));
                pte += PTX(va);
                if (!(*pte & PTE_P)) {
                  cprintf("\n0x%08x not mapped\n", va);
                  continue;
                }
                ph = PTE_ADDR(*pte) + PGOFF(va);

		cprintf("\n0x%08x -> 0x%08x\n", va, ph);

		// dump PDE and PTE flags
		dump_pde_flags(pde);
		if (pte)
			dump_pte_flags(pte);
	}

	cputchar('\n');
	return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
