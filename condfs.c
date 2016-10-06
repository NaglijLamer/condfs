#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include "condfs_structs.h"
#include "condfs.h"
#include "condfs_vncache.h"

/*
 * Init file system cache
 * Called during kldload execution
 */
int condfs_init(struct vfsconf *conf){
	PRINTF_DEBUG("%s\n", "INIT");
	mtx_assert(&Giant, MA_OWNED);
	condfs_vncache_init();
	return (0);
}

/*
 * Called during file system mounting
 */
int condfs_mount(struct mount *mp){
	PRINTF_DEBUG("%s\n", "MOUNT");
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

/*
 * Get root directory of file system
 */
int condfs_root(struct mount *mp, int flags, struct vnode **vpp){
	return (condfs_alloc_vnode(mp, vpp, -1, -1));
}

/*
 * Well... vfs will call mountpoint->mnt_stat
 */
int condfs_statfs(struct mount *mp, struct statfs *sbp){
	/*Nothing?*/
	return (0);
}

/*
 * Called during kldunload execution
 * Destroy all condinodes and free all vnodes
 * Uninit file system cache
 */ 
int condfs_uninit(struct vfsconf *conf){
	printf("%s\n", "UNINIT");
	condfs_purge_all();
	mtx_assert(&Giant, MA_OWNED); 
	condfs_vncache_uninit();
	return (0);
}

/*
 * Called during filesystem unmounting
 * Flush all vnodes, which belong to umomting mountpoint
 */
int condfs_unmount(struct mount *mp, int mntflags){
	printf("%s\n", "UNMOUNT");
	return (vflush(mp, 0, (mntflags & MNT_FORCE) ? FORCECLOSE : 0, curthread));
}

/*
 * Struct with vfs operations
 */
static struct vfsops condfs_vfsops = {
	.vfs_init = condfs_init,
	.vfs_mount = condfs_mount,
	.vfs_root = condfs_root,
	.vfs_statfs = condfs_statfs,
	.vfs_uninit = condfs_uninit,
	.vfs_unmount = condfs_unmount,
};

VFS_SET(condfs_vfsops, condfs, VFCF_SYNTHETIC | VFCF_JAIL);
