/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fake <linux/seq_file.h> for freestanding .ko builds.
 *
 * Declares the seq_file interface used by debugfs show callbacks and
 * DEFINE_SHOW_ATTRIBUTE.  The actual implementations are in the running
 * kernel and resolved at link/load time.
 */

#ifndef _FAKE_LINUX_SEQ_FILE_H
#define _FAKE_LINUX_SEQ_FILE_H

#include <linux/types.h>
#include <linux/uaccess.h>

/* seq_file — opaque; we only pass pointers to it. */
struct seq_file;

/* file — opaque; only used as a pointer in most callbacks. */
struct file;

/*
 * inode — opaque in freestanding builds.  The only use of inode* in this
 * project is in DEFINE_SHOW_ATTRIBUTE's generated __name_open function, which
 * reads inode->i_private.  Since debugfs_create_file is always called with
 * data=NULL in this project, i_private is always NULL.  The macro expansion
 * in debugfs.h avoids reading inode->i_private by passing NULL directly, so
 * struct inode need not be defined here — a forward declaration suffices.
 */
struct inode;

/* loff_t: kernel file offset type (signed 64-bit).
 * ssize_t: signed size type for read/write return values.
 * Neither is provided by the freestanding <types.h> shim (which only defines
 * uint*_t / int*_t / size_t / bool); add them here where first needed. */
#ifndef _LOFF_T_DEFINED
#define _LOFF_T_DEFINED
typedef long long loff_t;
#endif

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long ssize_t;
#endif

extern int     seq_printf(struct seq_file *m, const char *fmt, ...)
	       __attribute__((format(printf, 2, 3)));
extern int     single_open(struct file *file,
			   int (*show)(struct seq_file *, void *), void *data);
extern int     single_release(struct inode *inode, struct file *file);
extern ssize_t seq_read(struct file *file, char __user *buf,
			size_t size, loff_t *ppos);
extern loff_t  seq_lseek(struct file *file, loff_t offset, int whence);

#endif /* _FAKE_LINUX_SEQ_FILE_H */
