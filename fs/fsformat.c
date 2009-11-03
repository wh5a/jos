/*
 * JOS file system format
 */

#define _BSD_EXTENSION

// We don't actually want to define off_t!
#define off_t xxx_off_t
#define bool xxx_bool
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#undef off_t
#undef bool

// Prevent inc/types.h, included from inc/fs.h,
// from attempting to redefine types defined in the host's inttypes.h.
#define JOS_INC_TYPES_H
// Typedef the types that inc/mmu.h needs.
typedef uint32_t physaddr_t;
typedef uint32_t off_t;
typedef int bool;

#include <inc/mmu.h>
#include <inc/fs.h>

#define nelem(x)	(sizeof(x) / sizeof((x)[0]))
typedef struct Super Super;
typedef struct File File;

struct Super super;
int diskfd;
uint32_t nblocks;
uint32_t nbitblock;
uint32_t nextb;

enum {
	BLOCK_SUPER,
	BLOCK_DIR,
	BLOCK_FILE,
	BLOCK_BITS
};

struct Block
{
	uint32_t busy;
	uint32_t bno;
	uint32_t used;
	uint8_t buf[BLKSIZE];
	uint32_t type;
};

struct Block cache[16];

ssize_t
readn(int f, void *av, size_t n)
{
	uint8_t *a;
	size_t t;

	a = av;
	t = 0;
	while (t < n) {
		size_t m = read(f, a + t, n - t);
		if (m <= 0) {
			if (t == 0)
				return m;
			break;
		}
		t += m;
	}
	return t;
}

// make little-endian
void
swizzle(uint32_t *x)
{
	uint32_t y;
	uint8_t *z;

	z = (uint8_t*) x;
	y = *x;
	z[0] = y & 0xFF;
	z[1] = (y >> 8) & 0xFF;
	z[2] = (y >> 16) & 0xFF;
	z[3] = (y >> 24) & 0xFF;
}

void
swizzlefile(struct File *f)
{
	int i;

	if (f->f_name[0] == 0)
		return;
	swizzle((uint32_t*) &f->f_size);
	swizzle(&f->f_type);
	for (i = 0; i < NDIRECT; i++)
		swizzle(&f->f_direct[i]);
	swizzle(&f->f_indirect);
}

void
swizzleblock(struct Block *b)
{
	int i;
	struct Super *s;
	struct File *f;
	uint32_t *u;

	switch (b->type) {
	case BLOCK_SUPER:
		s = (struct Super*) b->buf;
		swizzle(&s->s_magic);
		swizzle(&s->s_nblocks);
		swizzlefile(&s->s_root);
		break;
	case BLOCK_DIR:
		f = (struct File*) b->buf;
		for (i = 0; i < BLKFILES; i++)
			swizzlefile(f + i);
		break;
	case BLOCK_BITS:
		u = (uint32_t*) b->buf;
		for (i = 0; i < BLKSIZE / 4; i++)
			swizzle(u + i);
		break;
	}
}

void
flushb(struct Block *b)
{
	swizzleblock(b);
	if (lseek(diskfd, b->bno * BLKSIZE, 0) < 0
	    || write(diskfd, b->buf, BLKSIZE) != BLKSIZE) {
		perror("flushb");
		fprintf(stderr, "\n");
		abort();
	}
	swizzleblock(b);
}

struct Block*
getblk(uint32_t bno, int clr, uint32_t type)
{
	int i, least;
	static int t = 1;
	struct Block *b;

	if (bno >= nblocks) {
		fprintf(stderr, "attempt to access past end of disk bno=%d\n", bno);
		abort();
	}

	least = -1;
	for (i = 0; i < nelem(cache); i++) {
		if (cache[i].bno == bno) {
			b = &cache[i];
			goto out;
		}
		if (!cache[i].busy
		    && (least == -1 || cache[i].used < cache[least].used))
			least = i;
	}

	if (least == -1) {
		fprintf(stderr, "panic: block cache full\n");
		abort();
	}

	b = &cache[least];
	if (b->used)
		flushb(b);

	if (lseek(diskfd, bno*BLKSIZE, 0) < 0
	    || readn(diskfd, b->buf, BLKSIZE) != BLKSIZE) {
		fprintf(stderr, "read block %d: ", bno);
		perror("");
		fprintf(stderr, "\n");
		abort();
	}
	b->bno = bno;
	if (!clr)
		swizzleblock(b);

out:
	if (clr)
		memset(b->buf, 0, sizeof(b->buf));
	b->used = ++t;
	if (b->busy) {
		fprintf(stderr, "panic: block #%d is busy\n", b->bno);
		abort();
	}
	/* it is important to reset b->type in case we reuse a block for a
	 * different purpose while it is still in the cache - this can happen
	 * for example if a file ends exactly on a block boundary */
	b->type = type;
	b->busy = 1;
	return b;
}

void
putblk(struct Block *b)
{
	b->busy = 0;
}

void
opendisk(const char *name)
{
	int i, r;
	struct Block *b;

	if ((diskfd = open(name, O_RDWR | O_CREAT, 0666)) < 0) {
		fprintf(stderr, "open %s: ", name);
		perror("");
		fprintf(stderr, "\n");
		abort();
	}

	if ((r = ftruncate(diskfd, 0)) < 0
	    || (r = ftruncate(diskfd, nblocks * BLKSIZE)) < 0) {
		fprintf(stderr, "truncate %s: ", name);
		perror("");
		abort();
	}

	nbitblock = (nblocks + BLKBITSIZE - 1) / BLKBITSIZE;
	for (i = 0; i < nbitblock; i++){
		b = getblk(2 + i, 0, BLOCK_BITS);
		memset(b->buf, 0xFF, BLKSIZE);
		putblk(b);
	}

	nextb = 2 + nbitblock;

	super.s_magic = FS_MAGIC;
	super.s_nblocks = nblocks;
	super.s_root.f_type = FTYPE_DIR;
	strcpy(super.s_root.f_name, "/");
}

void
storeblk(struct File *f, struct Block *b, int nblk)
{
	if (nblk < NDIRECT)
		f->f_direct[nblk] = b->bno;
	else if (nblk < NINDIRECT) {
		struct Block *bindir;
		if (f->f_indirect == 0) {
			bindir = getblk(nextb++, 1, BLOCK_BITS);
			f->f_indirect = bindir->bno;
		} else
			bindir = getblk(f->f_indirect, 0, BLOCK_BITS);
		((uint32_t*)bindir->buf)[nblk] = b->bno;
		putblk(bindir);
	} else {
		fprintf(stderr, "file too large\n");
		abort();
	}
}

struct File *
allocfile(struct File *dirf, const char *name, struct Block **dirb)
{
	struct File *ino;
	int nblk, i;

	nblk = (int)((dirf->f_size + BLKSIZE - 1) / BLKSIZE) - 1;
	if (nblk >= NDIRECT) {
		struct Block *idirb = getblk(dirf->f_indirect, 0, BLOCK_BITS);
		*dirb = getblk(((uint32_t*)idirb->buf) [nblk], 0, BLOCK_DIR);
		putblk(idirb);
	} else if (nblk >= 0)
		*dirb = getblk(dirf->f_direct[nblk], 0, BLOCK_DIR);
	else
		goto new_dirb;

	ino = (struct File *) (*dirb)->buf;
	for (i = 0; i < BLKFILES; i++)
		if (ino[i].f_name[0] == '\0') {
			ino = &ino[i];
			goto gotit;
		}

	putblk(*dirb);

new_dirb:
	*dirb = getblk(nextb++, 1, BLOCK_DIR);
	storeblk(dirf, *dirb, ++nblk);
	dirf->f_size += BLKSIZE;
	assert((nblk + 1) * BLKSIZE == dirf->f_size);
	
	ino = (struct File *) (*dirb)->buf;
	
gotit:
	strcpy(ino->f_name, name);
	return ino;
}

void
writefile(struct File *dirf, const char *name)
{
	int fd;
	const char *last;
	File *f;
	int n, nblk;
	struct Block *dirb, *b;

	if ((fd = open(name, O_RDONLY)) < 0) {
		fprintf(stderr, "open %s:", name);
		perror("");
		abort();
	}

	last = strrchr(name, '/');
	if (last)
		last++;
	else
		last = name;

	f = allocfile(dirf, last, &dirb);
	f->f_type = FTYPE_REG;

	n = 0;
	for (nblk = 0; ; nblk++) {
		b = getblk(nextb, 1, BLOCK_FILE);
		n = readn(fd, b->buf, BLKSIZE);
		if (n < 0) {
			fprintf(stderr, "reading %s: ", name);
			perror("");
			abort();
		}
		if (n == 0) {
			putblk(b);
			break;
		}
		nextb++;
		storeblk(f, b, nblk);
		putblk(b);
		if (n < BLKSIZE)
			break;
	}
	f->f_size = nblk*BLKSIZE + n;
	putblk(dirb);
}

void
writedirectory(struct File *parentdirf, char *name, int root)
{
	struct File *dirf;
	DIR *dir;
	struct dirent *ent;
	struct stat s;
	char pathbuf[PATH_MAX];
	int namelen;
	struct Block *dirb = NULL;

	if ((dir = opendir(name)) == NULL) {
		fprintf(stderr, "open %s:", name);
		perror("");
		abort();
	}

	if (!root) {
		const char *last = strrchr(name, '/');
		if (last)
			last++;
		else
			last = name;

		dirf = allocfile(parentdirf, last, &dirb);
		dirf->f_type = FTYPE_DIR;
		dirf->f_size = 0;
	} else
		dirf = parentdirf;

	strcpy(pathbuf, name);
	namelen = strlen(pathbuf);
	if (pathbuf[namelen - 1] != '/') {
		pathbuf[namelen++] = '/';
		pathbuf[namelen] = 0;
	}

	while ((ent = readdir(dir)) != NULL) {
		int ent_namlen = strlen(ent->d_name);
		strcpy(pathbuf + namelen, ent->d_name);

		// don't depend on unreliable parts of the dirent structure
		if (stat(pathbuf, &s) < 0)
			continue;
		
		if (S_ISREG(s.st_mode))
			writefile(dirf, pathbuf);
		else if (S_ISDIR(s.st_mode)
			 && (ent_namlen > 1 || ent->d_name[0] != '.')
			 && (ent_namlen > 2 || ent->d_name[0] != '.' || ent->d_name[1] != '.')
			 && (ent_namlen > 3 || ent->d_name[0] != 'C' || ent->d_name[1] != 'V' || ent->d_name[2] != 'S'))
			writedirectory(dirf, pathbuf, 0);
	}

	closedir(dir);
	if (dirb)
		putblk(dirb);
}

void
finishfs(void)
{
	int i;
	struct Block *b;

	for (i = 0; i < nextb; i++) {
		b = getblk(2 + i/BLKBITSIZE, 0, BLOCK_BITS);
		((uint32_t*)b->buf)[(i%BLKBITSIZE)/32] &= ~(1<<(i%32));
		putblk(b);
	}

	// this is slow but not too slow.  i do not care
	if (nblocks != nbitblock*BLKBITSIZE) {
		b = getblk(2+nbitblock-1, 0, BLOCK_BITS);
		for (i = nblocks % BLKBITSIZE; i < BLKBITSIZE; i++)
			((uint32_t*)b->buf)[i/32] &= ~(1<<(i%32));
		putblk(b);
	}

	b = getblk(1, 1, BLOCK_SUPER);
	memmove(b->buf, &super, sizeof(Super));
	putblk(b);
}

void
flushdisk(void)
{
	int i;

	for (i = 0; i < nelem(cache); i++)
		if (cache[i].used)
			flushb(&cache[i]);
}

void
usage(void)
{
	fprintf(stderr, "Usage: fsformat kern/fs.img NBLOCKS files...\n\
       fsformat kern/fs.img NBLOCKS -r DIR\n");
	abort();
}

int
main(int argc, char **argv)
{
	int i;
	char *s;

	assert(BLKSIZE % sizeof(struct File) == 0);

	if (argc < 3)
		usage();

	nblocks = strtol(argv[2], &s, 0);
	if (*s || s == argv[2] || nblocks < 2 || nblocks > 1024)
		usage();
	
	opendisk(argv[1]);

	if (strcmp(argv[3], "-r") == 0) {
		if (argc != 5)
			usage();
		writedirectory(&super.s_root, argv[4], 1);
	} else {
		for (i = 3; i < argc; i++)
			writefile(&super.s_root, argv[i]);
	}
	
	finishfs();
	flushdisk();
	exit(0);
	return 0;
}

