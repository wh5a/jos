// See COPYRIGHT for copyright information.

#ifndef JOS_INC_FS_H
#define JOS_INC_FS_H

#include <inc/types.h>

// File nodes (both in-memory and on-disk)

// Bytes per file system block - same as page size
#define BLKSIZE		PGSIZE
#define BLKBITSIZE	(BLKSIZE * 8)

// Maximum size of a filename (a single path component), including null
// Must be a multiple of 4
#define MAXNAMELEN	128

// Maximum size of a complete pathname, including null
#define MAXPATHLEN	1024

// Number of block pointers in a File descriptor
#define NDIRECT		10
// Number of direct block pointers in an indirect block
#define NINDIRECT	(BLKSIZE / 4)

#define MAXFILESIZE	(NINDIRECT * BLKSIZE)

struct File {
	char f_name[MAXNAMELEN];	// filename
	off_t f_size;			// file size in bytes
	uint32_t f_type;		// file type

	// Block pointers.
	// A block is allocated iff its value is != 0.
	uint32_t f_direct[NDIRECT];	// direct blocks
	uint32_t f_indirect;		// indirect block

	// Points to the directory in which this file lives.
	// Meaningful only in memory; the value on disk can be garbage.
	// dir_lookup() sets the value when required.
	struct File *f_dir;

	// Pad out to 256 bytes; must do arithmetic in case we're compiling
	// fsformat on a 64-bit machine.
	uint8_t f_pad[256 - MAXNAMELEN - 8 - 4*NDIRECT - 4 - sizeof(struct File*)];
} __attribute__((packed));	// required only on some 64-bit machines

// An inode block contains exactly BLKFILES 'struct File's
#define BLKFILES	(BLKSIZE / sizeof(struct File))

// File types
#define FTYPE_REG	0	// Regular file
#define FTYPE_DIR	1	// Directory


// File system super-block (both in-memory and on-disk)

#define FS_MAGIC	0x4A0530AE	// related vaguely to 'J\0S!'

struct Super {
	uint32_t s_magic;		// Magic number: FS_MAGIC
	uint32_t s_nblocks;		// Total number of blocks on disk
	struct File s_root;		// Root directory node
};

// Definitions for requests from clients to file system

#define FSREQ_OPEN	1
#define FSREQ_MAP	2
#define FSREQ_SET_SIZE	3
#define FSREQ_CLOSE	4
#define FSREQ_DIRTY	5
#define FSREQ_REMOVE	6
#define FSREQ_SYNC	7

struct Fsreq_open {
	char req_path[MAXPATHLEN];
	int req_omode;
};

struct Fsreq_map {
	int req_fileid;
	off_t req_offset;
};

struct Fsreq_set_size {
	int req_fileid;
	off_t req_size;
};

struct Fsreq_close {
	int req_fileid;
};

struct Fsreq_dirty {
	int req_fileid;
	off_t req_offset;
};

struct Fsreq_remove {
	char req_path[MAXPATHLEN];
};

#endif /* !JOS_INC_FS_H */
