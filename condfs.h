#ifndef _CONDFS_H
#define _CONDFS_H

#include <sys/vnode.h>

typedef enum {
	cfstype_none = 0,
	cfstype_root,
	cfstype_dir,
	cfstype_this,
	cfstype_parent,
	cfstype_file
} cfs_type_t;

struct cfs_node {
	char c_name[50];
	cfs_type_t c_type;
	int c_flags;
	struct mtx c_mutex;
	void *c_date;
	struct cfs_node *c_parent;
	struct cfs_node *c_nodes;
	struct cfs_node *c_next;
};

struct vop_vector cfs_vnodeops = {
	.vop_default = &default_vnodeops,
	.vop_access = VOP_EOPNOTSUPP,
	.vop_close = VOP_EOPNOTSUPP,
	.vop_cachedlookup = VOP_EOPNOTSUPP,
	.vop_create = VOP_EOPNOTSUPP,
	.vop_getattr = VOP_EOPNOTSUPP,
	.vop_getextattr = VOP_EOPNOTSUPP,
	.vop_ioctl = VOP_EOPNOTSUPP,
	.vop_link = VOP_EOPNOTSUPP,
	.vop_lookup = VOP_EOPNOTSUPP,
	.vop_mkdir = VOP_EOPNOTSUPP,
	.vop_mknod = VOP_EOPNOTSUPP,
	.vop_open = VOP_EOPNOTSUPP,
	.vop_read = VOP_EOPNOTSUPP,
	.vop_readdir = VOP_EOPNOTSUPP,
        .vop_readlink = VOP_EOPNOTSUPP,
	.vop_reclaim = VOP_EOPNOTSUPP,
	.vop_remove = VOP_EOPNOTSUPP,
	.vop_rename = VOP_EOPNOTSUPP,
	.vop_setattr = VOP_EOPNOTSUPP,
	.vop_symlink = VOP_EOPNOTSUPP,
	.vop_vptocnp = VOP_EOPNOTSUPP,
	.vop_write = VOP_EOPNOTSUPP,
};

int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, struct cfs_node *node);
int condfs_init(struct vfsconf *conf);
int condfs_mount(struct mount *mp);
int condfs_root(struct mount *mp, int flags, struct vnode **vpp);
int condfs_statfs(struct mount *mp, struct statfs *sbp);
int condfs_uninit(struct vfsconf *conf);
int condfs_unmount(struct mount *mp, int mntflags);

#endif
