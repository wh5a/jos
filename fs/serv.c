/*
 * File system server main loop -
 * serves IPC requests from other environments.
 */

#include <inc/x86.h>
#include <inc/string.h>

#include "fs.h"


#define debug 0

struct OpenFile {
	uint32_t o_fileid;	// file id
	struct File *o_file;	// mapped descriptor for open file
	int o_mode;		// open mode
	struct Fd *o_fd;	// Fd page
};

// Max number of open files in the file system at once
#define MAXOPEN		1024
#define FILEVA		0xD0000000

// initialize to force into data section
struct OpenFile opentab[MAXOPEN] = {
	{ 0, 0, 1, 0 }
};

// Virtual address at which to receive page mappings containing client requests.
#define REQVA		0x0ffff000

void
serve_init(void)
{
	int i;
	uintptr_t va = FILEVA;
	for (i = 0; i < MAXOPEN; i++) {
		opentab[i].o_fileid = i;
		opentab[i].o_fd = (struct Fd*) va;
		va += PGSIZE;
	}
}

// Allocate an open file.
int
openfile_alloc(struct OpenFile **o)
{
	int i, r;

	// Find an available open-file table entry
	for (i = 0; i < MAXOPEN; i++) {
		switch (pageref(opentab[i].o_fd)) {
		case 0:
			if ((r = sys_page_alloc(0, opentab[i].o_fd, PTE_P|PTE_U|PTE_W)) < 0)
				return r;
			/* fall through */
		case 1:
			opentab[i].o_fileid += MAXOPEN;
			*o = &opentab[i];
			memset(opentab[i].o_fd, 0, PGSIZE);
			return (*o)->o_fileid;
		}
	}
	return -E_MAX_OPEN;
}

// Look up an open file for envid.
int
openfile_lookup(envid_t envid, uint32_t fileid, struct OpenFile **po)
{
	struct OpenFile *o;

	o = &opentab[fileid % MAXOPEN];
	if (pageref(o->o_fd) == 1 || o->o_fileid != fileid)
		return -E_INVAL;
	*po = o;
	return 0;
}

// Serve requests, sending responses back to envid.
// To send a result back, ipc_send(envid, r, 0, 0).
// To include a page, ipc_send(envid, r, srcva, perm).
void
serve_open(envid_t envid, struct Fsreq_open *rq)
{
	char path[MAXPATHLEN];
	struct File *f;
	int fileid;
	int r;
	struct OpenFile *o;

	if (debug)
		cprintf("serve_open %08x %s 0x%x\n", envid, rq->req_path, rq->req_omode);

	// Copy in the path, making sure it's null-terminated
	memmove(path, rq->req_path, MAXPATHLEN);
	path[MAXPATHLEN-1] = 0;

	// Find an open file ID
	if ((r = openfile_alloc(&o)) < 0) {
		if (debug)
			cprintf("openfile_alloc failed: %e", r);
		goto out;
	}
	fileid = r;

	// Open the file
	if ((r = file_open(path, &f)) < 0) {
		if (debug)
			cprintf("file_open failed: %e", r);
		goto out;
	}

	// Save the file pointer
	o->o_file = f;

	// Fill out the Fd structure
	o->o_fd->fd_file.file = *f;
	o->o_fd->fd_file.id = o->o_fileid;
	o->o_fd->fd_omode = rq->req_omode;
	o->o_fd->fd_dev_id = devfile.dev_id;
	o->o_mode = rq->req_omode;

	if (debug)
		cprintf("sending success, page %08x\n", (uintptr_t) o->o_fd);
	ipc_send(envid, 0, o->o_fd, PTE_P|PTE_U|PTE_W);
	return;
out:
	ipc_send(envid, r, 0, 0);
}

void
serve_set_size(envid_t envid, struct Fsreq_set_size *rq)
{
	struct OpenFile *o;
	int r;
	
	if (debug)
		cprintf("serve_set_size %08x %08x %08x\n", envid, rq->req_fileid, rq->req_size);

	// The file system server maintains three structures
	// for each open file.
	//
	// 1. The on-disk 'struct File' is mapped into the part of memory
	//    that maps the disk.  This memory is kept private to the
	//    file server.
	// 2. Each open file has a 'struct Fd' as well,
	//    which sort of corresponds to a Unix file descriptor.
	//    This 'struct Fd' is kept on *its own page* in memory,
	//    and it is shared with any environments that
	//    have the file open.
	//    Part of the 'struct Fd' is a *copy* of the on-disk
	//    'struct File' (struct Fd::fd_file.file), except that the
	//    block pointers are effectively garbage.
	//    This lets environments find out a file's size by examining
	//    struct Fd::fd_file.file.f_size, for example.
	//    *The server must make sure to keep two copies of the
	//    'struct File' in sync!*
	// 3. 'struct OpenFile' links these other two structures,
	//    and is kept private to the file server.
	//    The server maintains an array of all open files, indexed
	//    by "file ID".
	//    (There can be at most MAXFILE files open concurrently.)
	//    The client uses file IDs to communicate with the server.
	//    File IDs are a lot like environment IDs in the kernel.
	//    Use openfile_lookup to translate file IDs to struct OpenFile.

	// Every file system IPC call has the same general structure.
	// Here's how it goes.

	// First, use openfile_lookup to find the relevant open file.
	// On failure, return the error code to the client with ipc_send.
	if ((r = openfile_lookup(envid, rq->req_fileid, &o)) < 0)
		goto out;

	// Second, call the relevant file system function (from fs/fs.c).
	// On failure, return the error code to the client.
	if ((r = file_set_size(o->o_file, rq->req_size)) < 0)
		goto out;

	// Third, update the 'struct Fd' copy of the 'struct File'
	// as appropriate.
	o->o_fd->fd_file.file.f_size = rq->req_size;

	// Finally, return to the client!
	// (We just return r since we know it's 0 at this point.)
out:
	ipc_send(envid, r, 0, 0);
}

// Map the requested block in the client's address space
// by using ipc_send.
void
serve_map(envid_t envid, struct Fsreq_map *rq)
{
	int r;
	char *blk = NULL;
	struct OpenFile *o;
	int perm = 0;

	if (debug)
		cprintf("serve_map %08x %08x %08x\n", envid, rq->req_fileid, rq->req_offset);

	if ((r = openfile_lookup(envid, rq->req_fileid, &o)) < 0)
		goto out;

	// Map read-only unless the file's open mode (o->o_mode) allows writes
	// (see the O_ flags in inc/lib.h).
        perm = PTE_U | PTE_P | PTE_SHARE;
        if (o->o_mode & (O_WRONLY|O_RDWR))
          perm |= PTE_W;
        
	r = file_get_block(o->o_file, rq->req_offset/BLKSIZE, &blk);

out:
	ipc_send(envid, r, blk, perm);
}

void
serve_close(envid_t envid, struct Fsreq_close *rq)
{
	struct OpenFile *o;
	int r;

	if (debug)
		cprintf("serve_close %08x %08x\n", envid, rq->req_fileid);

	if ((r = openfile_lookup(envid, rq->req_fileid, &o)) < 0)
		goto out;
	file_close(o->o_file);
	r = 0;

out:
	ipc_send(envid, r, 0, 0);
}

void
serve_remove(envid_t envid, struct Fsreq_remove *rq)
{
	char path[MAXPATHLEN];
	int r;

	if (debug)
		cprintf("serve_remove %08x %s\n", envid, rq->req_path);

	// Delete the named file.
	// Note: This request doesn't refer to an open file.
	// Hint: Make sure the path is null-terminated!

	// Copy in the path, making sure it's null-terminated
	memmove(path, rq->req_path, MAXPATHLEN);
	path[MAXPATHLEN-1] = 0;

	// Delete the specified file
	r = file_remove(path);
	ipc_send(envid, r, 0, 0);
}

// Mark the page containing the requested file offset as dirty.
void
serve_dirty(envid_t envid, struct Fsreq_dirty *rq)
{
	struct OpenFile *o;
	int r;

	if (debug)
		cprintf("serve_dirty %08x %08x %08x\n", envid, rq->req_fileid, rq->req_offset);

	if ((r = openfile_lookup(envid, rq->req_fileid, &o)) < 0)
		goto out;

	r = file_dirty(o->o_file, rq->req_offset);

out:
	ipc_send(envid, r, 0, 0);
}

void
serve_sync(envid_t envid)
{
	fs_sync();
	ipc_send(envid, 0, 0, 0);
}

void
serve(void)
{
	uint32_t req, whom;
	int perm;
	
	while (1) {
		perm = 0;
		req = ipc_recv((int32_t *) &whom, (void *) REQVA, &perm);
		if (debug)
			cprintf("fs req %d from %08x [page %08x: %s]\n",
				req, whom, vpt[VPN(REQVA)], REQVA);

		// All requests must contain an argument page
		if (!(perm & PTE_P)) {
			cprintf("Invalid request from %08x: no argument page\n",
				whom);
			continue; // just leave it hanging...
		}

		switch (req) {
		case FSREQ_OPEN:
			serve_open(whom, (struct Fsreq_open*)REQVA);
			break;
		case FSREQ_MAP:
			serve_map(whom, (struct Fsreq_map*)REQVA);
			break;
		case FSREQ_SET_SIZE:
			serve_set_size(whom, (struct Fsreq_set_size*)REQVA);
			break;
		case FSREQ_CLOSE:
			serve_close(whom, (struct Fsreq_close*)REQVA);
			break;
		case FSREQ_DIRTY:
			serve_dirty(whom, (struct Fsreq_dirty*)REQVA);
			break;
		case FSREQ_REMOVE:
			serve_remove(whom, (struct Fsreq_remove*)REQVA);
			break;
		case FSREQ_SYNC:
			serve_sync(whom);
			break;
		default:
			cprintf("Invalid request code %d from %08x\n", whom, req);
			break;
		}
		sys_page_unmap(0, (void*) REQVA);
	}
}

void
umain(void)
{
	static_assert(sizeof(struct File) == 256);
        binaryname = "fs";
	cprintf("FS is running\n");

	// Check that we are able to do I/O
	outw(0x8A00, 0x8A00);
	cprintf("FS can do I/O\n");

	serve_init();
	fs_init();
	fs_test();

	serve();
}

