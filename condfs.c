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
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/sbuf.h>
#include <sys/vnode.h>
#include <sys/condvar.h>
#include <sys/sleepqueue.h>
//#include <fs/pseudofs/pseudofs.h>
#include "condfs.h"
#include "condfs_vnops.h"

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

static MALLOC_DEFINE (M_CDFSN, "le", "le");

struct cfs_node root;

static void condfs_purge(struct cfs_node *node){
	if (node->vnp == NULL) return;
	printf("%s\n", "before hold");
	vhold(node->vnp);
	/*printf("%s\n", "before lock");
	VOP_LOCK(node->vnp, LK_EXCLUSIVE);*/
	printf("%s\n", "before vgone");
	vgone(node->vnp);
	/*printf("%s\n", "before unlock");
	VOP_UNLOCK(node->vnp, 0);*/
	printf("%s\n", "before vdrop");
	vdrop(node->vnp);
	printf("%s\n", "bye purge");
}

int condfs_init(struct vfsconf *conf){
	mtx_assert(&Giant, MA_OWNED);
	/*Here we must make our hierarchy...*/
	strlcpy(root.c_name, "/", sizeof(root.c_name));
	root.c_type = cfstype_root;
	root.vnp = NULL;
	struct cfs_node *this = malloc(sizeof(*this), M_CDFSN, M_WAITOK | M_ZERO);
	strlcpy(this->c_name, ".", sizeof(this->c_name));
	this->c_type = cfstype_this;
	this->c_parent = &root;
	this->c_next = root.c_nodes;
	root.c_nodes = this;
	
	struct cfs_node *par = malloc(sizeof(*par), M_CDFSN, M_WAITOK | M_ZERO);
	strlcpy(par->c_name, "..", sizeof(par->c_name));
	par->c_type = cfstype_parent;
	par->c_parent = &root;
	par->c_next = root.c_nodes;
	root.c_nodes = par;

	struct thread *t;
	struct proc *p = LIST_FIRST(&allproc);
	while (p != NULL){
		t = TAILQ_FIRST(&(p->p_threads));
		while (t != NULL){
			//printf("thread %d\n", t->td_tid);
			int test = sleepq_type(t->td_wchan);
			if (test != -1 && test & SLEEPQ_CONDVAR){
				printf("thread %d with label %s\n", t->td_tid, t->td_wmesg);
				if (strcmp(t->td_wmesg, "fukc") == 0){
					printf("%s\n", "Be FReeeeeeee");
					cv_signal((struct cv*)t->td_wchan);
				}
			}
			t = TAILQ_NEXT(t, td_plist);
		}
		p = LIST_NEXT(p, p_list);
	}

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
	if (root.vnp != NULL) { 
		printf("%d %s\n", root.vnp->v_holdcnt, "good but not"); 
		VI_LOCK(root.vnp); 
		vget(root.vnp, LK_EXCLUSIVE | LK_INTERLOCK, curthread);
		*vpp = root.vnp; 
		cache_purge(root.vnp);
		return (0);
	}
	int error = condfs_alloc_vnode(mp, vpp, &root);
	if (!error) root.vnp = *vpp;
	return (error);
}

int condfs_statfs(struct mount *mp, struct statfs *sbp){
	/*Nothing?*/
	return (0);
}

int condfs_uninit(struct vfsconf *conf){
	mtx_assert(&Giant, MA_OWNED); /*WHAT is THIS??*/
	free(root.c_nodes->c_next, M_CDFSN);
	free(root.c_nodes, M_CDFSN);
	//condfs_purge(&root);
	return (0);
}

int condfs_unmount(struct mount *mp, int mntflags){
	root.vnp = NULL;
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

//DECLARE_MODULE(condfs, condfs_conf, SI_SUB_EXEC, SI_ORDER_FIRST);
