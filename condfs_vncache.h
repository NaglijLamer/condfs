#ifndef _CONDFS_VNCACHE_H
#define _CONDFS_VNCACHE_H

#include "condfs_structs.h"

#define CDFS_CACHE_RET(retstatus) \
	do { \
		mtx_unlock(&cache_mutex); \
		return (retstatus); \
	} while (0)

#define CONDFS_PURGE_CONDINODE(inode) condfs_purge(inode, -1)
#define CONDFS_PURGE_PID(pid) condfs_purge(NULL, pid)

int create_condinode(struct condinode **inode, cfs_type type, struct vnode *vnp, lwpid_t tid, pid_t pid);
int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, pid_t pid, lwpid_t tid);
void condfs_purge_all(void);
void condfs_purge(struct condinode *inode, pid_t pid);
int condfs_free_condinode(struct vnode *vp);
void condfs_vncache_init(void);
void condfs_vncache_uninit(void);

#endif
