#include <inc/fs.h>
#include <inc/lib.h>

#define debug 0

extern uint8_t fsipcbuf[PGSIZE];	// page-aligned, declared in entry.S

// Send an IP request to the file server, and wait for a reply.
// type: request code, passed as the simple integer IPC value.
// fsreq: page to send containing additional request data, usually fsipcbuf.
//	  Can be modified by server to return additional response info.
// dstva: virtual address at which to receive reply page, 0 if none.
// *perm: permissions of received page.
// Returns 0 if successful, < 0 on failure.
static int
fsipc(unsigned type, void *fsreq, void *dstva, int *perm)
{
	envid_t whom;

	if (debug)
		cprintf("[%08x] fsipc %d %08x\n", env->env_id, type, fsipcbuf);

	ipc_send(envs[1].env_id, type, fsreq, PTE_P | PTE_W | PTE_U);
	return ipc_recv(&whom, dstva, perm);
}

// Send file-open request to the file server.
// Includes 'path' and 'omode' in request,
// and on reply maps the returned file descriptor page
// at the address indicated by the caller in 'fd'.
// Returns 0 on success, < 0 on failure.
int
fsipc_open(const char *path, int omode, struct Fd *fd)
{
	struct Fsreq_open *req;

	req = (struct Fsreq_open*)fsipcbuf;
	if (strlen(path) >= MAXPATHLEN)
		return -E_BAD_PATH;
	strcpy(req->req_path, path);
	req->req_omode = omode;

	return fsipc(FSREQ_OPEN, req, fd, 0);
}

// Make a map-block request to the file server.
// We send the fileid and the (byte) offset of the desired block in the file,
// and the server sends us back a mapping for a page containing that block.
// Returns 0 on success, < 0 on failure.
int
fsipc_map(int fileid, off_t offset, void *dstva)
{
  struct Fsreq_map *req = (struct Fsreq_map*)fsipcbuf;
  req->req_fileid = fileid;
  req->req_offset = offset;
  return fsipc(FSREQ_MAP, req, dstva, 0);
}

// Make a set-file-size request to the file server.
int
fsipc_set_size(int fileid, off_t size)
{
	struct Fsreq_set_size *req;

	req = (struct Fsreq_set_size*) fsipcbuf;
	req->req_fileid = fileid;
	req->req_size = size;
	return fsipc(FSREQ_SET_SIZE, req, 0, 0);
}

// Make a file-close request to the file server.
// After this the fileid is invalid.
int
fsipc_close(int fileid)
{
	struct Fsreq_close *req;

	req = (struct Fsreq_close*) fsipcbuf;
	req->req_fileid = fileid;
	return fsipc(FSREQ_CLOSE, req, 0, 0);
}

// Ask the file server to mark a particular file block dirty.
int
fsipc_dirty(int fileid, off_t offset)
{
  struct Fsreq_dirty *req = (struct Fsreq_dirty*)fsipcbuf;
  req->req_fileid = fileid;
  req->req_offset = offset;
  return fsipc(FSREQ_DIRTY, req, 0, 0);
}

// Ask the file server to delete a file, given its pathname.
int
fsipc_remove(const char *path)
{
	struct Fsreq_remove *req;

	req = (struct Fsreq_remove*) fsipcbuf;
	if (strlen(path) >= MAXPATHLEN)
		return -E_BAD_PATH;
	strcpy(req->req_path, path);
	return fsipc(FSREQ_REMOVE, req, 0, 0);
}

// Ask the file server to update the disk
// by writing any dirty blocks in the buffer cache.
int
fsipc_sync(void)
{
	return fsipc(FSREQ_SYNC, fsipcbuf, 0, 0);
}

