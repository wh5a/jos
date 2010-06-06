#include <inc/ns.h>
#include <inc/lib.h>
#include <lwip/sockets.h>

#define debug 0

// Virtual address at which to receive page mappings containing client requests.
#define REQVA		0x0ffff000
extern uint8_t nsipcbuf[PGSIZE];	// page-aligned, declared in entry.S

// Send an IP request to the network server, and wait for a reply.
// type: request code, passed as the simple integer IPC value.
// fsreq: page to send containing additional request data, usually fsipcbuf.
//	  Can be modified by server to return additional response info.
// dstva: virtual address at which to receive reply page, 0 if none.
// *perm: permissions of received page.
// Returns 0 if successful, < 0 on failure.
static int
nsipc(unsigned type, void *fsreq, void *dstva, int *perm)
{
	envid_t whom;

	if (debug)
		cprintf("[%08x] nsipc %d %08x\n", env->env_id, type, nsipcbuf);

	ipc_send(envs[2].env_id, type, fsreq, PTE_P|PTE_W|PTE_U);
	return ipc_recv(&whom, dstva, perm);
}

int
nsipc_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
	int perm, r;
	struct Nsreq_accept *req;
	struct Nsret_accept *ret;
	
	req = (struct Nsreq_accept*)nsipcbuf;
	req->req_s = s;
	
	r = nsipc(NSREQ_ACCEPT, req, (void *)REQVA, &perm);
	
	ret = (struct Nsret_accept*) REQVA;
	memmove(addr, &ret->ret_addr, ret->ret_addrlen);
	*addrlen = ret->ret_addrlen;
	
	return r;
}

int
nsipc_bind(int s, struct sockaddr *name, socklen_t namelen)
{
	int perm;
	struct Nsreq_bind *req;
	
	req = (struct Nsreq_bind*)nsipcbuf;
	req->req_s = s;
	memmove(&req->req_name, name, namelen);
	req->req_namelen = namelen;
	return nsipc(NSREQ_BIND, req, 0, &perm);
}

int
nsipc_shutdown(int s, int how)
{
	int perm;
	struct Nsreq_shutdown *req;
	
	req = (struct Nsreq_shutdown*)nsipcbuf;
	req->req_s = s;
	req->req_how = how;
	return nsipc(NSREQ_SHUTDOWN, req, 0, &perm);
}

int
nsipc_close(int s)
{
	int perm;
	struct Nsreq_close *req;
	
	req = (struct Nsreq_close*)nsipcbuf;
	req->req_s = s;
	return nsipc(NSREQ_CLOSE, req, 0, &perm);
}

int
nsipc_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
	int perm;
	struct Nsreq_connect *req;
	
	req = (struct Nsreq_connect*)nsipcbuf;
	req->req_s = s;
	memmove(&req->req_name, name, namelen);
	req->req_namelen = namelen;
	return nsipc(NSREQ_CONNECT, req, 0, &perm);
}

int
nsipc_listen(int s, int backlog)
{
	int perm;
	struct Nsreq_listen *req;
	
	req = (struct Nsreq_listen*)nsipcbuf;
	req->req_s = s;
	req->req_backlog = backlog;
	return nsipc(NSREQ_LISTEN, req, 0, &perm);
}

int
nsipc_recv(int s, void *mem, int len, unsigned int flags)
{
	int perm, r;
	struct Nsreq_recv *req;
	void *ret;
	
	req = (struct Nsreq_recv*)nsipcbuf;
	req->req_s = s;
	req->req_len = len;
	req->req_flags = flags;
	
	r = nsipc(NSREQ_RECV, req, (void *)REQVA, &perm);
	
	assert(r < 1600 && r <= len);
	ret = (void *) REQVA;
	memmove(mem, ret, r);
	
	return r;
}

int
nsipc_send(int s, const void *dataptr, int size, unsigned int flags)
{
	int perm;
	struct Nsreq_send *req;
	
	req = (struct Nsreq_send*)nsipcbuf;
	req->req_s = s;
	assert(size < 1600);
	memmove(&req->req_dataptr, dataptr, size);
	req->req_size = size;
	req->req_flags = flags;
	return nsipc(NSREQ_SEND, req, 0, &perm);
}

int
nsipc_socket(int domain, int type, int protocol)
{
	int perm;
	struct Nsreq_socket *req;
	
	req = (struct Nsreq_socket*)nsipcbuf;
	req->req_domain = domain;
	req->req_type = type;
	req->req_protocol = protocol;
	return nsipc(NSREQ_SOCKET, req, 0, &perm);
}

