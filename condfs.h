#ifndef _CONDFS_H
#define _CONDFS_H

#include <sys/vnode.h>
#include <condfs_structs.h>

//struct cfs_node root;

int condfs_init(struct vfsconf *conf);
int condfs_mount(struct mount *mp);
int condfs_root(struct mount *mp, int flags, struct vnode **vpp);
int condfs_statfs(struct mount *mp, struct statfs *sbp);
int condfs_uninit(struct vfsconf *conf);
int condfs_unmount(struct mount *mp, int mntflags);

#endif
