#ifndef _CONDFS_VNOPS_H
#define _CONDFS_VNOPS_H

//int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, struct cfs_node *node);
int test_condvar(struct thread **thr, struct condinode *inode);

#endif
