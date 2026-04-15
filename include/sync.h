/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _SYNC_H
#define _SYNC_H

#ifdef CONFIG_KH_CHAIN_RCU

int  kh_sync_init(void);
void sync_cleanup(void);
void kh_sync_read_lock(void);
void kh_sync_read_unlock(void);
void kh_sync_write_lock(void);
void kh_sync_write_unlock(void);

#else

static inline int  kh_sync_init(void) { return 0; }
static inline void sync_cleanup(void) {}
static inline void kh_sync_read_lock(void) {}
static inline void kh_sync_read_unlock(void) {}
static inline void kh_sync_write_lock(void) {}
static inline void kh_sync_write_unlock(void) {}

#endif /* CONFIG_KH_CHAIN_RCU */

#endif /* _SYNC_H */
