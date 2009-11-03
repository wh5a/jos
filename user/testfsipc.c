#include <inc/lib.h>

int
strecmp(const char *a, const char *b)
{
	while (*b)
		if (*a++ != *b++)
			return 1;
	return 0;
}

const char *msg = "This is the NEW message of the day!\n\n";

#define FVA (struct Fd*)0xCCCCC000

void
umain(void)
{
	int r;
	int fileid;
	struct Fd *fd;

	if ((r = fsipc_open("/not-found", O_RDONLY, FVA)) < 0 && r != -E_NOT_FOUND)
		panic("serve_open /not-found: %e", r);
	else if (r == 0)
		panic("serve_open /not-found succeeded!");

	if ((r = fsipc_open("/newmotd", O_RDONLY, FVA)) < 0)
		panic("serve_open /newmotd: %e", r);
	fd = (struct Fd*) FVA;
	if (strlen(msg) != fd->fd_file.file.f_size)
		panic("serve_open returned size %d wanted %d\n", fd->fd_file.file.f_size, strlen(msg));
	cprintf("serve_open is good\n");

	if ((r = fsipc_map(fd->fd_file.id, 0, UTEMP)) < 0)
		panic("serve_map: %e", r);
	if (strecmp(UTEMP, msg) != 0)
		panic("serve_map returned wrong data");
	cprintf("serve_map is good\n");

	if ((r = fsipc_close(fd->fd_file.id)) < 0)
		panic("serve_close: %e", r);
	cprintf("serve_close is good\n");
	fileid = fd->fd_file.id;
	sys_page_unmap(0, (void*) FVA);

	if ((r = fsipc_map(fileid, 0, UTEMP)) != -E_INVAL)
		panic("serve_map does not handle stale fileids correctly");
	cprintf("stale fileid is good\n");
}

