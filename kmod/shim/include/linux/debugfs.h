/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/debugfs.h> for freestanding .ko builds.
 *
 * Declares debugfs_create_dir / _file / _remove_recursive and provides
 * the DEFINE_SHOW_ATTRIBUTE macro expansion (uses single_open from
 * <linux/seq_file.h>).
 */

#ifndef _FAKE_LINUX_DEBUGFS_H
#define _FAKE_LINUX_DEBUGFS_H

#include <linux/types.h>
#include <linux/seq_file.h>

/* Opaque forward declaration for dentry — only used as a pointer. */
struct dentry;

/*
 * file_operations — full layout matching the kernel struct for arm64 GKI.
 *
 * Every field must be in the correct order so that the kernel reads the right
 * function pointer from the right offset.  Unused callbacks are void * (NULL
 * at static init time) — the kernel treats NULL as "not implemented".
 *
 * Layout is stable across GKI 4.4–6.12 arm64 (LP64, 8 bytes per pointer).
 * Fields: owner, llseek, read, write, read_iter, write_iter, iopoll,
 *   iterate_shared, poll, unlocked_ioctl, compat_ioctl, mmap,
 *   mmap_supported_flags, open, flush, release, fsync, fasync,
 *   lock, sendpage, get_unmapped_area, check_flags, setfl, flock,
 *   splice_write, splice_read, splice_eof, fallocate, show_fdinfo,
 *   copy_file_range, remap_file_range, fadvise, uring_cmd,
 *   uring_cmd_iopoll.
 *
 * NOTE: between GKI 4.x and 6.x the field `iterate` was replaced by
 * `iterate_shared` (4.19+).  We only target GKI 5.10+ so only
 * `iterate_shared` is listed.
 */
struct module;  /* forward decl for owner */
struct file_operations {
	struct module *owner;
	loff_t   (*llseek)(struct file *, loff_t, int);
	ssize_t  (*read)(struct file *, char __user *, size_t, loff_t *);
	ssize_t  (*write)(struct file *, const char __user *, size_t, loff_t *);
	void     *read_iter;
	void     *write_iter;
	void     *iopoll;
	void     *iterate_shared;
	void     *poll;
	void     *unlocked_ioctl;
	void     *compat_ioctl;
	void     *mmap;
	unsigned long mmap_supported_flags;
	int      (*open)(struct inode *, struct file *);
	void     *flush;
	int      (*release)(struct inode *, struct file *);
	void     *fsync;
	void     *fasync;
	void     *lock;
	void     *sendpage;
	void     *get_unmapped_area;
	void     *check_flags;
	void     *setfl;
	void     *flock;
	void     *splice_write;
	void     *splice_read;
	void     *splice_eof;
	void     *fallocate;
	void     *show_fdinfo;
	void     *copy_file_range;
	void     *remap_file_range;
	void     *fadvise;
	void     *uring_cmd;
	void     *uring_cmd_iopoll;
};

extern struct dentry *debugfs_create_dir(const char *name,
					 struct dentry *parent);
extern struct dentry *debugfs_create_file(const char *name,
					  unsigned short mode,
					  struct dentry *parent, void *data,
					  const struct file_operations *fops);
extern void debugfs_remove_recursive(struct dentry *dentry);

/* IS_ERR_OR_NULL — true if ptr is NULL or an ERR_PTR (value in [-4095, -1]).
 * Matches kernel include/linux/err.h definition. */
#ifndef IS_ERR_OR_NULL
static inline int IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || ((unsigned long)ptr >= (unsigned long)-4095UL);
}
#endif

/* errno values used in the debugfs write handlers.
 * Match kernel <uapi/asm-generic/errno-base.h>. */
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EFAULT
#define EFAULT 14
#endif

/*
 * DEFINE_SHOW_ATTRIBUTE(__name) — expands to:
 *   static int __name_open(struct inode *, struct file *)
 *   static const struct file_operations __name_fops
 *
 * This mirrors the real kernel macro from <linux/seq_file.h>.
 * Placed here (next to debugfs_create_file) because it is exclusively
 * used to create debugfs read-only files.
 *
 * Freestanding deviation: the real macro passes inode->i_private as the data
 * argument to single_open.  In this project, debugfs_create_file is always
 * called with data=NULL, so i_private is always NULL.  We pass NULL directly
 * to avoid needing a layout-correct definition of struct inode.
 */
#define DEFINE_SHOW_ATTRIBUTE(__name)					\
	static int __name##_open(struct inode *inode, struct file *file)\
	{								\
		(void)inode;						\
		return single_open(file, __name##_show, NULL);		\
	}								\
	static const struct file_operations __name##_fops = {		\
		.open    = __name##_open,				\
		.read    = seq_read,					\
		.llseek  = seq_lseek,					\
		.release = single_release,				\
	}

#endif /* _FAKE_LINUX_DEBUGFS_H */
