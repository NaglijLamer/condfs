#include <sys/types.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mount.h>
#include <sys/param.h>
//#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/sbuf.h>
#include <sys/vnode.h>
//#include <fs/pseudofs/pseudofs.h>
#include "condfs.h"

/*static int cevent_handler(struct module *module, int event, void *arg){
	switch (event){
		case MOD_LOAD:
			printf("%s\n", "HOLY SHIIIIIII");
			break;
		case MOD_UNLOAD:
		case MOD_SHUTDOWN:
			printf("%s\n", "LOLE BYE");
			break;
		default:
			return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t condfs_conf = {
	"condfs",
	cevent_handler,
	NULL,
};*/

int condfs_init(struct vfsconf *conf){
	return (0);
}

int condfs_mount(struct mount *mp){
	struct statfs *sbp;
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);
	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	MNT_IUNLOCK(mp);
	vfs_getnewfsid(mp);
	sbp = &mp->mnt_stat;
	vfs_mountedfrom(mp, "condfs");
	sbp->f_bsize = PAGE_SIZE;
	sbp->f_iosize = PAGE_SIZE;
	sbp->f_blocks = 1;
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 1;
	sbp->f_ffree = 0;
	return (0);
}

int condfs_root(struct mount *mp, int flags, struct vnode **vpp){
	return (0);
}

int condfs_statfs(struct mount *mp, struct statfs *sbp){
	return (0);
}

int condfs_uninit(struct vfsconf *conf){
	return (0);
}

int condfs_unmount(struct mount *mp, int mntflags){
	return (0);
}

static struct vfsops condfs_vfsops = {
	.vfs_init = condfs_init,
	.vfs_mount = condfs_mount,
	.vfs_root = condfs_root,
	.vfs_statfs = condfs_statfs,
	.vfs_uninit = condfs_uninit,
	.vfs_unmount = condfs_unmount,
};

VFS_SET(condfs_vfsops, condfs, VFCF_SYNTHETIC | VFCF_JAIL);

//DECLARE_MODULE(condfs, condfs_conf, SI_SUB_EXEC, SI_ORDER_FIRST);
