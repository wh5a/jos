#include <inc/fs.h>
#include <inc/lib.h>

#define SECTSIZE	512			// bytes per disk sector
#define BLKSECTS	(BLKSIZE / SECTSIZE)	// sectors per block

/* Disk block n, when in memory, is mapped into the file system
 * server's address space at DISKMAP + (n*BLKSIZE). */
#define DISKMAP		0x10000000

/* Maximum disk size we can handle (3GB) */
#define DISKSIZE	0xC0000000

/* ide.c */
bool	ide_probe_disk1(void);
void	ide_set_disk(int diskno);
int	ide_read(uint32_t secno, void *dst, size_t nsecs);
int	ide_write(uint32_t secno, const void *src, size_t nsecs);

/* fs.c */
int	file_create(const char *path, struct File **f);
int	file_open(const char *path, struct File **f);
int	file_get_block(struct File *f, uint32_t file_blockno, char **pblk);
int	file_set_size(struct File *f, off_t newsize);
void	file_flush(struct File *f);
void	file_close(struct File *f);
int	file_remove(const char *path);
void	fs_init(void);
int	file_dirty(struct File *f, off_t offset);
void	fs_sync(void);

extern uint32_t *bitmap;
int	map_block(uint32_t);
int	alloc_block(void);

/* test.c */
void	fs_test(void);

