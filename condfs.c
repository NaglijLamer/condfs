#include <sys/types.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/sbuf.h>
#include <sys/vnode.h>
#include <sys/condvar.h>
#include <sys/sleepqueue.h>
#include "condfs.h"
#include "condfs_vnops.h"
#include "condfs_vncache.h"

static MALLOC_DEFINE (M_CDFSN, "le", "le");

//struct cfs_node root;
struct condinode *rooti;
//struct mtx cache_mutex;

int condfs_init(struct vfsconf *conf){
	printf("%s\n", "INIT");
	mtx_assert(&Giant, MA_OWNED);
	condfs_vncache_init();
	
	//create_condinode(rooti, cfstype_root, NULL, NULL, NULL);
	//mtx_init(&cache_mutex, "condfs vnodecache mutex", NULL, MTX_DEF);

	/*struct thread *t;
	struct proc *p = LIST_FIRST(&allproc);
	while (p != NULL){
		t = TAILQ_FIRST(&(p->p_threads));
		while (t != NULL){
			int test = sleepq_type(t->td_wchan);
			if (test != -1 && test & SLEEPQ_CONDVAR){
				printf("thread %d with label %s\n", t->td_tid, t->td_wmesg);
				if (strcmp(t->td_wmesg, "fukc") == 0){
					printf("%s\n", "Be FReeeeeeee");
					//cv_signal((struct cv*)t->td_wchan);
				}
			}
			t = TAILQ_NEXT(t, td_plist);
		}
		p = LIST_NEXT(p, p_list);
	}*/

	return (0);
}

int condfs_mount(struct mount *mp){
	printf("%s\n", "MOUNT");
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
	/*if (root.vnp != NULL) { 
		VI_LOCK(root.vnp); 
		vget(root.vnp, LK_EXCLUSIVE | LK_INTERLOCK, curthread);
		*vpp = root.vnp; 
		cache_purge(root.vnp);
		return (0);
	}
	int error = condfs_alloc_vnode(mp, vpp, &root);
	if (!error) root.vnp = *vpp;
	return (error);*/

	return (condfs_alloc_vnode(mp, vpp, -1, -1));
}

int condfs_statfs(struct mount *mp, struct statfs *sbp){
	/*Nothing?*/
	return (0);
}

int condfs_uninit(struct vfsconf *conf){
	printf("%s\n", "UNINIT");
	condfs_purge_all();
	mtx_assert(&Giant, MA_OWNED); 
	condfs_vncache_uninit();
	return (0);
}

int condfs_unmount(struct mount *mp, int mntflags){
	printf("%s\n", "UNMOUNT");
	//root.vnp = root.condvnp = NULL;
	return (vflush(mp, 0, (mntflags & MNT_FORCE) ? FORCECLOSE : 0, curthread));
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
