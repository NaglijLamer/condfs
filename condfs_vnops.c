#include <sys/ctype.h>
#include <sys/types.h>
#include <sys/dirent.h>
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
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/condvar.h>
#include <sys/sleepqueue.h>
#include <sys/namei.h>
#include "condfs.h"
#include "condfs_structs.h"
#include "condfs_vnops.h"

struct vop_vector cfs_vnodeops;
extern struct cfs_node root;

int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, struct cfs_node *node){
	int error = getnewvnode("condfs", mp, &cfs_vnodeops, vpp);
	if (error) return (error);
	/*cfs_type_t type = (node == NULL) ? cfstype_file : node->c_type;
	switch (type) {
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
	}*/
	if (node == NULL) {
		printf("%s\n", "REG");
		(*vpp)->v_type = VREG;
	}
	else {
		printf("%s\n", "ROOT");
		(*vpp)->v_type = VDIR;
		(*vpp)->v_vflag = VV_ROOT;
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
	printf("%s\n", "Getattr");
	struct vattr *vap = va->a_vap;
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
	vap->va_mode = 0666; /*I think, I should change it*/
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
	/*I think it is not necessary to call vaccess()*/
	printf("%s\n", "Access");
	return (0);
}

static int cdfs_iterate(struct proc **p, struct thread **t){
        /*
	 * Iterate by all condvars, set to **t threads with active condvars.
         * If t is NULL then get first thread.
         * If not NULL - get next.
         * If t is NULL and we not find any procs - return some error?
	 */
        //printf("%s\n", "Iterator");
	sx_assert(&allproc_lock, SX_SLOCKED);
        if (*p == NULL) *p = LIST_FIRST(&allproc);
        while (*p != NULL){
                *t = (*t == NULL) ? TAILQ_FIRST(&((*p)->p_threads)) : TAILQ_NEXT((*t), td_plist);
                while (*t != NULL){
                        int test = sleepq_type((*t)->td_wchan);
                        if (test != -1 && test & SLEEPQ_CONDVAR){
                                /*We got condvar!*/
                                return (0);
                        }
                        *t = TAILQ_NEXT((*t), td_plist);
                }
                *p = LIST_NEXT((*p), p_list);
        }
        return (-1);
}


static int cdfs_lookup(struct vop_cachedlookup_args *va){
	struct componentname *cnp = va->a_cnp;
	struct mount *mp;
	int thr, i;
	printf("%s\n", "Lookup");
	if (va->a_dvp->v_type != VDIR)
		return (ENOTDIR);
	if ((cnp->cn_flags & ISLASTCN) &&
		(cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EOPNOTSUPP); /*Not supp DELETE, RENAME ops*/
	/*self*/
	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.'){
		*(va->a_vpp) = va->a_dvp;
		VREF(va->a_dvp);
		return (0);
	}
	mp = va->a_dvp->v_mount;
	if (cnp->cn_flags & ISDOTDOT){
		return (EIO); /*I think so...but I can mistake*/
	}
	/*Check, is it a condvar*/
	for (thr = 0, i = 0; i < cnp->cn_namelen && isdigit(cnp->cn_nameptr[i]); ++i)
		if ((thr = thr * 10 + cnp->cn_nameptr[i] - '0') > PID_MAX + NO_PID)
			break;
	if (i != cnp->cn_namelen) return (ENOENT);
	sx_slock(&allproc_lock);
	struct proc *p = NULL;
	struct thread *t = NULL;
	while(cdfs_iterate(&p, &t) != -1) {
		if (thr == t->td_tid) {
			if (root.condvnp != NULL){
				VI_LOCK(root.condvnp);
				vget(root.condvnp, LK_EXCLUSIVE | LK_INTERLOCK, curthread);
				*(va->a_vpp) = root.condvnp;
				cache_purge(root.condvnp);
			}
			else {
				condfs_alloc_vnode(mp, (va->a_vpp), NULL);
				root.condvnp = *(va->a_vpp);
			}
			if (cnp->cn_flags & ISDOTDOT) {
				vn_lock(va->a_dvp, LK_EXCLUSIVE | LK_RETRY);
				if (va->a_dvp->v_iflag & VI_DOOMED) {
					vput(*(va->a_vpp));
					 *(va->a_vpp) = NULL;
					sx_sunlock(&allproc_lock);
					return (ENOENT);
				}
			}
			if (cnp->cn_flags & MAKEENTRY && !(va->a_dvp->v_iflag & VI_DOOMED))
				cache_enter(va->a_dvp, *(va->a_vpp), cnp);
			sx_sunlock(&allproc_lock);
			(*(va->a_vpp))->v_data = cnp->cn_nameptr;
			return (0);	
		}
	}
	sx_sunlock(&allproc_lock);
	return (ENOENT);
}
struct cdfsentry {
	STAILQ_ENTRY(cdfsentry) link;
	struct dirent entry;
};
STAILQ_HEAD(cdfsdirentlist, cdfsentry);

static int cdfs_readdir(struct vop_readdir_args *va){
	struct cdfsentry *cdfsent, *cdfsent2;
	struct cdfsdirentlist lst;
	struct uio *uio = va->a_uio;
	off_t offset;
	int resid;
	int error = 0;
	printf("%s\n", "Readdir");
	STAILQ_INIT(&lst);
	if (va->a_vp->v_type != VDIR)
		return (ENOTDIR);
	offset = va->a_uio->uio_offset;
	resid = va->a_uio->uio_resid;
	if (offset < 0 || offset % 32 != 0 || (resid && resid < 32)) /*32? Magick!*/
		return (EINVAL);
	if (resid == 0)
		return (0);
	/*
	 * some iterate logic
	 * call cdfs_iterate();
	 */
	sx_slock(&allproc_lock);
	struct proc *p = NULL;
	struct thread *t = NULL;
	/*skip smth*/
	for (; offset > 0; offset -= 32){
		if (cdfs_iterate(&p, &t) == -1){
			sx_sunlock(&allproc_lock);
			return (0);
		}
	}

	while (cdfs_iterate(&p, &t) != -1 && resid >= 32){
		if ((cdfsent = malloc(sizeof(struct cdfsentry), M_IOV,
			M_NOWAIT | M_ZERO)) == NULL) {
				error = ENOMEM;
				break;
		}
		cdfsent->entry.d_reclen = 32;
		cdfsent->entry.d_fileno = t->td_tid;
		char name[100];
		sprintf(name, "%d", t->td_tid);
		strcpy(cdfsent->entry.d_name, name);
		cdfsent->entry.d_namlen = strlen(name);
		/*we shoulds check type of file?? Which file?*/
		STAILQ_INSERT_TAIL(&lst, cdfsent, link);
		offset += 32;
		resid -= 32;
	}
	int i = 0;
	STAILQ_FOREACH_SAFE(cdfsent, &lst, link, cdfsent2) {
		if (error == 0)
			error = uiomove(&cdfsent->entry, 32, uio);
		free(cdfsent, M_IOV);
		i++;
	}
	sx_sunlock(&allproc_lock);
	return (error);
}

static int cdfs_write(struct vop_write_args *va){
	printf("%s\n", "Write");
	struct vnode *vn = va->a_vp;
	struct sbuf sb;
	struct uio *uio =va->a_uio;
	int error, thr, i;
	if (vn->v_type != VREG)
		return (EINVAL);
	sbuf_uionew(&sb, uio, &error);
	if (error)
		return (error);
	//printf("%s\n", sbuf_data(&sb));
	if (strcmp(sbuf_data(&sb), "signal") == 0){
	 	for (thr = 0, i = 0; i < strlen((char*)(vn->v_data)); ++i)
			thr = thr * 10 + ((char*)(vn->v_data))[i] - '0';
		//printf("%d\n", thr);
		sx_slock(&allproc_lock);
		struct proc *p = NULL;
		struct thread *t = NULL;
		while(cdfs_iterate(&p, &t) != -1){
			if (thr == t->td_tid){
				sbuf_delete(&sb);
				cv_signal(t->td_wchan);
				sx_sunlock(&allproc_lock);
				return (0);
			}
		}

	}
	sbuf_delete(&sb);
	sx_sunlock(&allproc_lock);
	return(ENOENT);
}

static int cdfs_setattr(struct vop_setattr_args *va){
	/*I think, it is stupid, but...*/
	printf("%s\n", "Setattr");
	//return (EOPNOTSUPP);
	return (0);
}

static int cdfs_vptocnp(struct vop_vptocnp_args *ap){
	printf("%s\n", "VPTOCNOP");
	if (ap->a_vp->v_type == VDIR){
		*(ap->a_vpp) = ap->a_vp;
		vhold(*(ap->a_vpp));
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
	//.vop_write = VOP_EOPNOTSUPP,
	.vop_write = cdfs_write,
};
