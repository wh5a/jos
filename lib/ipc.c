// User-level IPC library routines

#include <inc/lib.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
//
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
  void *dstva = pg ? pg : (void *)UTOP;
  int r = sys_ipc_recv(dstva);

  if (from_env_store)
    *from_env_store = r ? 0 : env->env_ipc_from;
  if (perm_store)
    *perm_store = r ? 0 : env->env_ipc_perm;

  if (r)
    return r;
  else
    return env->env_ipc_value;
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
  void *srcva = pg ? pg : (void *)UTOP;
  int r;
  while (1) {
    r = sys_ipc_try_send(to_env, val, srcva, perm);
    if (r == -E_IPC_NOT_RECV)
//   Use sys_yield() to be CPU-friendly.
      sys_yield();
    else if (r < 0)
      panic("sys_ipc_try_send: %e\n", r);
    else
      return;
  }
}
