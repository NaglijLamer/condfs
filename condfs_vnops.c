#include <sys/types.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/sysproto.h>
#include <sys/sbuf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include "condfs_structs.h"
#include "condfs_vnops.h"

struct vop_vector cfs_vnodeops;

int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, struct cfs_node *node){
	int error = getnewvnode("condfs", mp, &cfs_vnodeops, vpp);
	if (error) return (error);
	switch (node->c_type) {
		case cfstype_root:
			(*vpp)->v_vflag = VV_ROOT;
			printf("%s\n", "Cuc");
		case cfstype_dir:
		case cfstype_this:
		case cfstype_parent:
			(*vpp)->v_type = VDIR;
			break;
		case cfstype_file:
			(*vpp)->v_type = VREG;
		case cfstype_none:
		default:
			panic("%s\n", "What the kcuf?");
	}
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
	VN_LOCK_AREC(*vpp);
	error = insmntque(*vpp, mp);
	if (error)
		*vpp = NULLVP;
	return (error);
}
		

static int cdfs_reclaim(struct vop_reclaim_args *va){
	return (0);
}

static int cdfs_getattr(struct vop_getattr_args *va){
	struct vattr *vap = va->a_vap;
	printf("%s\n", "smbd call it?");
	vap->va_type = va->a_vp->v_type;
	vap->va_fileid = 2;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_bytes = vap->va_size = 
	vap->va_filerev = 0;
	vap->va_fsid = va->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_nlink = 1;
	nanotime(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;
	vap->va_mode = 0555; /*I think, I should change it*/
	vap->va_uid = 0;
	vap->va_gid = 0;
	return (0);
}

static int cdfs_open(struct vop_open_args *va){
	printf("%s\n", "Open");
	return (0);
}

static int cdfs_close(struct vop_close_args *va){
	printf("%s\n", "Close");
	return (0);
}

static int cdfs_access(struct vop_access_args *va){
	printf("%s\n", "Access");
	/*dumb*/
	return (0);
}

static int cdfs_lookup(struct vop_cachedlookup_args *va){
	printf("%s\n", "Lookup");
	/*dumb*/
	return (0);
}

static int cdfs_readdir(struct vop_readdir_args *va){
	printf("%s\n", "Readdir");
	/*dumb*/
	return (0);
}

static int cdfs_setattr(struct vop_setattr_args *va){
	/*I think, it is stupid, but...*/
	printf("%s\n", "Setattr");
	return (EOPNOTSUPP);
}

static int cdfs_vptocnp(struct vop_vptocnp_args *ap){
	printf("%s\n", "VPTOCNOP");
	if (ap->a_vp->v_type == VDIR){
		*(ap->a_vpp) = ap->a_vp;
		return (0);
	}
	return (ENOMEM);
}

struct vop_vector cfs_vnodeops = {
	.vop_default = &default_vnodeops,
	//.vop_access = VOP_EOPNOTSUPP,
	.vop_access = cdfs_access,
	//.vop_close = VOP_EOPNOTSUPP,
	.vop_close = cdfs_close,
	//.vop_cachedlookup = VOP_EOPNOTSUPP,
	.vop_cachedlookup = cdfs_lookup,
	.vop_create = VOP_EOPNOTSUPP,
	//.vop_getattr = VOP_EOPNOTSUPP,
	.vop_getattr = cdfs_getattr,
	.vop_getextattr = VOP_EOPNOTSUPP,
	.vop_ioctl = VOP_EOPNOTSUPP,
	.vop_link = VOP_EOPNOTSUPP,
	//.vop_lookup = VOP_EOPNOTSUPP,
	.vop_lookup = vfs_cache_lookup,
	.vop_mkdir = VOP_EOPNOTSUPP,
	.vop_mknod = VOP_EOPNOTSUPP,
	//.vop_open = VOP_EOPNOTSUPP,
	.vop_open = cdfs_open,
	.vop_read = VOP_EOPNOTSUPP,
	//.vop_readdir = VOP_EOPNOTSUPP,
	.vop_readdir = cdfs_readdir,
	.vop_readlink = VOP_EOPNOTSUPP,
	//.vop_reclaim = VOP_EOPNOTSUPP,
	.vop_reclaim = cdfs_reclaim,
	.vop_remove = VOP_EOPNOTSUPP,
	.vop_rename = VOP_EOPNOTSUPP,
	//.vop_setattr = VOP_EOPNOTSUPP,
	.vop_setattr = cdfs_setattr,
	.vop_symlink = VOP_EOPNOTSUPP,
	//.vop_vptocnp = VOP_EOPNOTSUPP,
	.vop_vptocnp = cdfs_vptocnp,
	.vop_write = VOP_EOPNOTSUPP,
};
