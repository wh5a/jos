#include <inc/fs.h>
#include <inc/string.h>
#include <inc/lib.h>

#define debug 0

static int file_close(struct Fd *fd);
static ssize_t file_read(struct Fd *fd, void *buf, size_t n, off_t offset);
static ssize_t file_write(struct Fd *fd, const void *buf, size_t n, off_t offset);
static int file_stat(struct Fd *fd, struct Stat *stat);
static int file_trunc(struct Fd *fd, off_t newsize);

struct Dev devfile =
{
	.dev_id =	'f',
	.dev_name =	"file",
	.dev_read =	file_read,
	.dev_write =	file_write,
	.dev_close =	file_close,
	.dev_stat =	file_stat,
	.dev_trunc =	file_trunc
};

// Helper functions for file access
static int fmap(struct Fd *fd, off_t oldsize, off_t newsize);
static int funmap(struct Fd *fd, off_t oldsize, off_t newsize, bool dirty);

// Open a file (or directory),
// returning the file descriptor index on success, < 0 on failure.
int
open(const char *path, int mode)
{
	// Find an unused file descriptor page using fd_alloc.
  struct Fd *fd;
  int r = fd_alloc(&fd);
  if (r)
    return r;
	// Then send a message to the file server to open a file
	// using a function in fsipc.c.
	// (fd_alloc does not allocate a page, it just returns an
	// unused fd address.  Do you need to allocate a page?  Look
	// at fsipc.c if you aren't sure.)
  if ((r = fsipc_open(path, mode, fd)))
    goto err;
	// Then map the file data (you may find fmap() helpful).
  if ((r = fmap(fd, 0, fd->fd_file.file.f_size)))
    goto err;
	// Return the file descriptor index.
  return fd2num(fd);
	// If any step fails, use fd_close to free the file descriptor.
err:
  fd_close(fd, 0);
  return r;
}

// Clean up a file-server file descriptor.
// This function is called by fd_close.
static int
file_close(struct Fd *fd)
{
	// Unmap any data mapped for the file,
  int r1 = funmap(fd, fd->fd_file.file.f_size, 0, 1);
	// then tell the file server that we have closed the file
	// (to free up its resources).
  int r2 = fsipc_close(fd->fd_file.id);
  return (r1 || r2);
}

// Read 'n' bytes from 'fd' at the current seek position into 'buf'.
// Since files are memory-mapped, this amounts to a memmove()
// surrounded by a little red tape to handle the file size and seek pointer.
static ssize_t
file_read(struct Fd *fd, void *buf, size_t n, off_t offset)
{
	size_t size;

	// avoid reading past the end of file
	size = fd->fd_file.file.f_size;
	if (offset > size)
		return 0;
	if (offset + n > size)
		n = size - offset;

	// read the data by copying from the file mapping
	memmove(buf, fd2data(fd) + offset, n);
	return n;
}

// Find the page that maps the file block starting at 'offset',
// and store its address in '*blk'.
int
read_map(int fdnum, off_t offset, void **blk)
{
	int r;
	char *va;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd)) < 0)
		return r;
	if (fd->fd_dev_id != devfile.dev_id)
		return -E_INVAL;
	va = fd2data(fd) + offset;
	if (offset >= MAXFILESIZE)
		return -E_NO_DISK;
	if (!(vpd[PDX(va)] & PTE_P) || !(vpt[VPN(va)] & PTE_P))
		return -E_NO_DISK;
	*blk = (void*) va;
	return 0;
}

// Write 'n' bytes from 'buf' to 'fd' at the current seek position.
static ssize_t
file_write(struct Fd *fd, const void *buf, size_t n, off_t offset)
{
	int r;
	size_t tot;

	// don't write past the maximum file size
	tot = offset + n;
	if (tot > MAXFILESIZE)
		return -E_NO_DISK;

	// increase the file's size if necessary
	if (tot > fd->fd_file.file.f_size) {
		if ((r = file_trunc(fd, tot)) < 0)
			return r;
	}

	// write the data
	memmove(fd2data(fd) + offset, buf, n);
	return n;
}

static int
file_stat(struct Fd *fd, struct Stat *st)
{
	strcpy(st->st_name, fd->fd_file.file.f_name);
	st->st_size = fd->fd_file.file.f_size;
	st->st_isdir = (fd->fd_file.file.f_type == FTYPE_DIR);
	return 0;
}

// Truncate or extend an open file to 'size' bytes
static int
file_trunc(struct Fd *fd, off_t newsize)
{
	int r;
	off_t oldsize;
	uint32_t fileid;

	if (newsize > MAXFILESIZE)
		return -E_NO_DISK;

	fileid = fd->fd_file.id;
	oldsize = fd->fd_file.file.f_size;
	if ((r = fsipc_set_size(fileid, newsize)) < 0)
		return r;
	assert(fd->fd_file.file.f_size == newsize);

	if ((r = fmap(fd, oldsize, newsize)) < 0)
		return r;
	funmap(fd, oldsize, newsize, 0);

	return 0;
}

// Call the file system server to obtain and map file pages
// when the size of the file as mapped in our memory increases.
// Harmlessly does nothing if oldsize >= newsize.
// Returns 0 on success, < 0 on error.
// If there is an error, unmaps any newly allocated pages.
static int
fmap(struct Fd* fd, off_t oldsize, off_t newsize)
{
  uint32_t fileid = fd->fd_file.id;
  off_t off = ROUNDUP(oldsize, PGSIZE);
  char *va = fd2data(fd);
  int r;
  
  for (; off < newsize; off+=PGSIZE) {
    r = fsipc_map(fileid, off, va+off);
    if (r) {
      funmap(fd, off, oldsize, 0);
      return r;
    }
  }
  return 0;
}

// Unmap any file pages that no longer represent valid file pages
// when the size of the file as mapped in our address space decreases.
// Harmlessly does nothing if newsize >= oldsize.
//
// Hint: Remember to call fsipc_dirty if dirty is true and PTE_D bit
// is set in the pagetable entry.
// Hint: Use fd2data to get the start of the file mapping area for fd.
// Hint: You can use ROUNDUP to page-align offsets.
static int
funmap(struct Fd* fd, off_t oldsize, off_t newsize, bool dirty)
{
  uint32_t fileid = fd->fd_file.id;
  off_t off = ROUNDUP(newsize, PGSIZE);
  char *va = fd2data(fd);
  int r, ret = 0;

  for (; off < oldsize; off+=PGSIZE) {
    if (dirty && (vpt[VPN(va+off)] & PTE_D)) {
      r = fsipc_dirty(fileid, off);
      if (r)
        ret = r;
    }
    sys_page_unmap(0, va+off);
  }
  return ret;
}

// Delete a file
int
remove(const char *path)
{
	return fsipc_remove(path);
}

// Synchronize disk with buffer cache
int
sync(void)
{
	return fsipc_sync();
}

