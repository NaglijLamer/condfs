#include <sys/ctype.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/sleepqueue.h>
#include <sys/namei.h>
#include <sys/limits.h>
#include "condfs_structs.h"
#include "condfs_vnops.h"
#include "condfs_vncache.h"

struct vop_vector cfs_vnodeops;

int test_condvar(struct thread **thr, struct condinode *inode){
	if (inode->tid == -1 && inode->pid == -1) return (0);
	if ((*thr = tdfind(inode->tid, inode->pid)) == NULL) return (-1);
		//goto bad;
	thread_lock(*thr);
	int test = sleepq_type((*thr)->td_wchan);
	if (!(test != -1 && test & SLEEPQ_CONDVAR)) {
		thread_unlock(*thr);
		return (-1);
		goto bad;
	}
	return (0);
bad:
	/*
	 * If we want to call condinode_purge,
	 * we should unlock vnode
	 */
	//CONDFS_PURGE_CONDINODE(inode);
	return (-1);
}
		

static int cdfs_reclaim(struct vop_reclaim_args *va){
	PRINTF_DEBUG("%d %s %d\n", curthread->td_tid, "Reclaim",
		 ((struct condinode*)va->a_vp->v_data)->tid);
	return (condfs_free_condinode(va->a_vp));
}

static int cdfs_getattr(struct vop_getattr_args *va){
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Getattr");
	struct vnode *vp = va->a_vp;
	struct condinode *inode = (struct condinode*)vp->v_data;
	struct vattr *vap = va->a_vap;
	struct thread *thr = NULL;
	if (test_condvar(&thr, inode) != 0) {
		if (thr != NULL) PROC_UNLOCK(thr->td_proc);
		return (ENOENT);
	}
	vap->va_fileid = inode->tid < 0 ? 2 : inode->tid;
	if (thr != NULL) {
		thread_unlock(thr);
		PROC_UNLOCK(thr->td_proc);
	}
	vap->va_type = va->a_vp->v_type;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_bytes = vap->va_size = 0;
	vap->va_filerev = 0;
	vap->va_fsid = va->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_nlink = 0;
	nanotime(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;
	vap->va_mode = 0666; 
	vap->va_uid = 0;
	vap->va_gid = 0;
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "out GETATTR");
	return (0);
}

static int cdfs_open(struct vop_open_args *va){
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Open");
	return (0);
}

static int cdfs_close(struct vop_close_args *va){
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Close");
	return (0);
}

static int cdfs_access(struct vop_access_args *va){
	/*I think it is not necessary to call vaccess()*/
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Access");
	return (0);
}

static int cdfs_iterate(struct proc **p, struct thread **t){
        /*
	 * Iterate by all condvars, set to **t threads with active condvars.
         * If t is NULL then get first thread.
         * If not NULL - get next.
         * If t is NULL and we not find any procs - return some error?
	 * N.B. Call PRINTF_DEBUG can be really bad for debug
	 */
        //PRINTF_DEBUG("%s\n", "Iterator");
	sx_assert(&allproc_lock, SX_SLOCKED);
        if (*p == NULL) *p = LIST_FIRST(&allproc);
        while (*p != NULL){
		if (PROC_LOCKED(*p) == 0)
			PROC_LOCK(*p);
                *t = (*t == NULL) ? TAILQ_FIRST(&((*p)->p_threads)) : TAILQ_NEXT((*t), td_plist);
                while (*t != NULL){
			thread_lock(*t);
                        int test = sleepq_type((*t)->td_wchan);
			thread_unlock(*t);
                        if (test != -1 && test & SLEEPQ_CONDVAR){
                                /*We got condvar!*/
                                return (0);
                        }
                        *t = TAILQ_NEXT((*t), td_plist);
                }
		PROC_UNLOCK(*p);
                *p = LIST_NEXT((*p), p_list);
        }
        return (-1);
}

static int cdfs_fastlookup(struct vop_lookup_args *va){
	struct componentname *cnp = va->a_cnp;
	struct mount *mp;
	int thr, i;
	PRINTF_DEBUG("%d Fastlookup %s\n", curthread->td_tid, cnp->cn_nameptr);
	if (va->a_dvp->v_type != VDIR)
		return (ENOTDIR);
	if ((cnp->cn_flags & ISLASTCN) &&
		(cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EOPNOTSUPP);
	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.'){
		*(va->a_vpp) = va->a_dvp;
		VREF(va->a_dvp);
		PRINTF_DEBUG("%d %s\n", curthread->td_tid, "out LOOKUP it is a .");
		return (0);
	}
	mp = va->a_dvp->v_mount;
	if (cnp->cn_flags & ISDOTDOT){
		return (EIO);
	}
	for (thr = 0, i = 0; i < cnp->cn_namelen && isdigit(cnp->cn_nameptr[i]); ++i)
		thr = thr * 10 + cnp->cn_nameptr[i] - '0';
	if (i != cnp->cn_namelen) return (ENOENT);
	sx_slock(&allproc_lock);
	struct proc *p = NULL;
	struct thread *t = NULL;
	while(cdfs_iterate(&p, &t) != -1) {
		if (thr == t->td_tid) {
			pid_t pid = p->p_pid;
			lwpid_t tid = t->td_tid;
			PROC_UNLOCK(p);
			sx_sunlock(&allproc_lock);
			condfs_alloc_vnode(mp, (va->a_vpp), pid, tid);
			if (cnp->cn_flags & ISDOTDOT) {
				vn_lock(va->a_dvp, LK_EXCLUSIVE | LK_RETRY);
				if (va->a_dvp->v_iflag & VI_DOOMED) {
					vput(*(va->a_vpp));
					*(va->a_vpp) = NULL;
					return (ENOENT);
				}
			}
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
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Readdir");
	STAILQ_INIT(&lst);
	if (va->a_vp->v_type != VDIR)
		return (ENOTDIR);
	offset = va->a_uio->uio_offset;
	resid = va->a_uio->uio_resid;
	if (offset < 0 || offset % MAGIC_SIZE != 0 || (resid && resid < MAGIC_SIZE)) 
		return (EINVAL);
	if (resid == 0)
		return (0);
	/*
	 * Some iterate logic
	 * Call cdfs_iterate();
	 */
	sx_slock(&allproc_lock);
	struct proc *p = NULL;
	struct thread *t = NULL;
	/*kostyl*/
	int kost = 1;
	for (; offset > 0; offset -= MAGIC_SIZE){
		if (cdfs_iterate(&p, &t) == -1){
			kost--;
			offset -= MAGIC_SIZE;
			if (offset <= 0) break;
			sx_sunlock(&allproc_lock);
			PRINTF_DEBUG("%d %s\n", curthread->td_tid, "out READDIR nth to read");
			return (0);
		}
	}

	while (kost == 1 && (cdfs_iterate(&p, &t) != -1 && resid >= MAGIC_SIZE)){
		ADD_DIRECTORY_CDFSENTRY(t->td_tid, DT_REG, "%d", t->td_tid);
	}
	sx_sunlock(&allproc_lock);
	if (kost == 1 && resid >= MAGIC_SIZE){
		ADD_DIRECTORY_CDFSENTRY(3, DT_DIR, "%s", "..");
	}
	if (resid >= MAGIC_SIZE){
		ADD_DIRECTORY_CDFSENTRY(2, DT_DIR, "%s", ".");
	}

	int i = 0;
	STAILQ_FOREACH_SAFE(cdfsent, &lst, link, cdfsent2) {
		if (error == 0)
			error = uiomove(&cdfsent->entry, MAGIC_SIZE, uio);
		free(cdfsent, M_IOV);
		i++;
	}
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "out READDIR");
	return (error);
}

static int cdfs_read(struct vop_read_args *va){
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Read");
	struct vnode *vp = va->a_vp;
	struct condinode *inode = (struct condinode*)vp->v_data;
	struct sbuf *sb = NULL;
	struct uio *uio = va->a_uio;
	struct thread *thr = NULL;
	int error;
	off_t buflen;
	if (uio->uio_resid < 0 || uio->uio_offset < 0 ||
		uio->uio_resid > OFF_MAX - uio->uio_offset)
			return (EINVAL);
	if (vp->v_type != VREG)
		return (EINVAL);

	buflen = uio->uio_offset + uio->uio_resid;
	if (buflen > MAXPHYS)
		buflen = MAXPHYS;
	sb = sbuf_new(sb, NULL, buflen + 1, 0);
	if (test_condvar(&thr, inode) != 0) {
		if (thr != NULL) PROC_UNLOCK(thr->td_proc);
		sbuf_delete(sb);
		return (ENOENT);
	}
	sbuf_printf(sb, "%d: %s\n", inode->tid, thr->td_wmesg);
	thread_unlock(thr);
	PROC_UNLOCK(thr->td_proc);
	PRINTF_DEBUG("%s\n", thr->td_wmesg);
	buflen = sbuf_len(sb);
	error = uiomove_frombuf(sbuf_data(sb), buflen, uio);
	sbuf_delete(sb);
	return (error);
}

static int cdfs_write(struct vop_write_args *va){
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Write");
	struct vnode *vp = va->a_vp;
	struct condinode *inode = (struct condinode*)vp->v_data;
	struct sbuf sb;
	struct uio *uio = va->a_uio;
	struct thread *thr = NULL;
	struct cv *cvp;
	int error;
	if (vp->v_type != VREG)
		return (EINVAL);
	sbuf_uionew(&sb, uio, &error);
	if (error) return (error);
	if (strcmp(sbuf_data(&sb), "signal") == 0){
		if (test_condvar(&thr, inode) != 0) {
			if (thr != NULL) PROC_UNLOCK(thr->td_proc);
			sbuf_delete(&sb);
			return (ENOENT);
		}
		cvp = thr->td_wchan;
		thread_unlock(thr);
		PROC_UNLOCK(thr->td_proc);
		cv_signal(cvp);
	}
	sbuf_delete(&sb);
	return (0);
}

static int cdfs_setattr(struct vop_setattr_args *va){
	/*I think, it is stupid, but...*/
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Setattr");
	return (0);
}

/* Remove this function?..*/
static int cdfs_vptocnp(struct vop_vptocnp_args *ap){
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "VPTOCNOP");
	if (ap->a_vp->v_type == VDIR){
		*(ap->a_vpp) = ap->a_vp;
		vhold(*(ap->a_vpp));
		return (0);
	}
	return (ENOMEM);
}

struct vop_vector cfs_vnodeops = {
	.vop_default = &default_vnodeops,
	.vop_access = cdfs_access,
	.vop_close = cdfs_close,
	.vop_cachedlookup = VOP_EOPNOTSUPP,
	.vop_create = VOP_EOPNOTSUPP,
	.vop_getattr = cdfs_getattr,
	.vop_getextattr = VOP_EOPNOTSUPP,
	.vop_ioctl = VOP_EOPNOTSUPP,
	.vop_link = VOP_EOPNOTSUPP,
	.vop_lookup = cdfs_fastlookup,
	.vop_mkdir = VOP_EOPNOTSUPP,
	.vop_mknod = VOP_EOPNOTSUPP,
	.vop_open = cdfs_open,
	.vop_read = cdfs_read,
	.vop_readdir = cdfs_readdir,
	.vop_readlink = VOP_EOPNOTSUPP,
	.vop_reclaim = cdfs_reclaim,
	.vop_remove = VOP_EOPNOTSUPP,
	.vop_rename = VOP_EOPNOTSUPP,
	.vop_setattr = cdfs_setattr,
	.vop_symlink = VOP_EOPNOTSUPP,
	.vop_vptocnp = cdfs_vptocnp,
	.vop_write = cdfs_write,
};
