// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	if ((err&FEC_WR) && (vpt[VPN(addr)]&PTE_COW)) {
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
          if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
            panic("sys_page_alloc: %e", r);
          void *rounded = ROUNDDOWN(addr, PGSIZE);
          memmove(PFTEMP, rounded, PGSIZE);
          if ((r = sys_page_map(0, PFTEMP, 0, rounded, PTE_P|PTE_U|PTE_W)) < 0)
            panic("sys_page_map: %e", r);
          if ((r = sys_page_unmap(0, PFTEMP)) < 0)
            panic("sys_page_unmap: %e", r);
          return;
        }

	panic("Fatal page fault");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why might we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
  int r;
  int pte = vpt[pn];
  void *addr = (void *)(pn << PGSHIFT);
  // Note that you should use PTE_USER, not PTE_FLAGS, to mask out the relevant bits from the page table entry. PTE_FLAGS picks up the accessed and dirty bits as well.
  int perm = pte & PTE_USER;

  // If the page table entry has the PTE_SHARE bit set, just copy the mapping directly. 
  if (!(perm&PTE_SHARE) && (perm&PTE_W || perm&PTE_COW)) {
    perm &= ~PTE_W;
    perm |= PTE_COW;
    r = sys_page_map(0, addr, envid, addr, perm);
    if (r < 0)
      return r;

    /* Marking our own page COW must be the very last thing of this function.
       If we did this before mapping into child, weird consequences would happen when duping stack:
       1. Mark our page COW, which happens to be the stack
       2. Push arguments to sys_page_map() on stack, which triggers pgfault
       3. Make a new copy and mark it writable
       4. Return to the point where we were about to map into child
       5. Parent and child now share a stack which is COW by child, but writable by parent!
    */
    // The above reasoning also answers why we mark COW again if it was COW at the beginning.
    return sys_page_map(0, addr, 0, addr, perm);
  }
  return sys_page_map(0, addr, envid, addr, perm);
}

envid_t realfork(int shared) {
// Set up our page fault handler appropriately.
  set_pgfault_handler(pgfault);
// Create a child.
  envid_t envid = sys_exofork();
  if (envid < 0)
    panic("sys_exofork: %e", envid);
  if (envid == 0) { // Child
    // Fix env
    env = envs + ENVX(sys_getenvid());  // Problematic for sfork because globals are shared
    return 0;
  }

  // Parent
  int r;
  int pdeno, pteno;
  uint32_t pn = 0;

  for (pdeno = 0; pdeno < VPD(UTOP); pdeno++) {
    if (vpd[pdeno] == 0) {
      // skip empty PDEs
      pn += NPTENTRIES;
      continue;
    }

    for (pteno = 0; pteno < NPTENTRIES; pteno++,pn++) {
      if (vpt[pn] == 0) {
        // skip empty PTEs
        continue;
      }

      // Do not duplicate the exception stack
      if (pn == VPN(UXSTACKTOP) - 1)
        continue;

      if (!shared || pn == VPN(USTACKTOP) - 1) {
        r = duppage(envid, pn);
        if (r)
          panic("duppage: %e", r);
      }
      else {
        void *addr = (void *)(pn << PGSHIFT);
        int perm = vpt[pn] & PTE_USER;
        perm |= PTE_SHARE;
        r = sys_page_map(0, addr, 0, addr, perm);
        if (r)
          panic("sys_page_map: %e", r);
        r = sys_page_map(0, addr, envid, addr, perm);
        if (r)
          panic("sys_page_map: %e", r);
      }
    }
  }

  r = sys_page_alloc(envid, (void*) (UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W);
  if (r)
    panic("sys_page_alloc: %e", r);
  sys_env_set_pgfault_upcall(envid, env->env_pgfault_upcall);

// Mark the child as runnable and return.
  if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
    panic("sys_env_set_status: %e", r);

  return envid;
}

//
// User-level fork with copy-on-write.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
envid_t
fork(void)
{
  return realfork(0);
}

// The parent and child share all their memory pages except the stack area
envid_t
sfork(void)
{
  return realfork(1);
}
