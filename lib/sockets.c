#include <inc/lib.h>
#include <lwip/sockets.h>

int
accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
	return nsipc_accept(s, addr, addrlen);
}

int
bind(int s, struct sockaddr *name, socklen_t namelen)
{
	return nsipc_bind(s, name, namelen);
}

int
shutdown(int s, int how)
{
	return nsipc_shutdown(s, how);
}

int
closesocket(int s)
{
	return nsipc_close(s);
}

int
connect(int s, const struct sockaddr *name, socklen_t namelen)
{
	return nsipc_connect(s, name, namelen);
}

int
listen(int s, int backlog)
{
	return nsipc_listen(s, backlog);
}

int
recv(int s, void *mem, int len, unsigned int flags)
{
	return nsipc_recv(s, mem, len, flags);
}

int
send(int s, const void *dataptr, int size, unsigned int flags)
{
	return nsipc_send(s, dataptr, size, flags);
}

int
socket(int domain, int type, int protocol)
{
	return nsipc_socket(domain, type, protocol);
}
