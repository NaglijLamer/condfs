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
#include <sys/limits.h>
#include "condfs.h"
#include "condfs_structs.h"
#include "condfs_vnops.h"
#include "condfs_vncache.h"

struct vop_vector cfs_vnodeops;
//extern struct cfs_node root;
extern struct condinode rooti;
static MALLOC_DEFINE (M_CDFSN, "le", "le");

//static const char* fs_name = "condfs";

/*int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, struct cfs_node *node){
	int error = getnewvnode(fs_name, mp, &cfs_vnodeops, vpp);
	if (error) return (error);
	if (node == NULL) {
		(*vpp)->v_type = VREG;
	}
	else {
		(*vpp)->v_type = VDIR;
		(*vpp)->v_vflag = VV_ROOT;
	}
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
	VN_LOCK_AREC(*vpp);
	error = insmntque(*vpp, mp);
	if (error)
		*vpp = NULLVP;
	return (error);
}*/


int test_condvar(struct thread **thr, struct condinode *inode){
	//printf("%d %d\n", inode->tid, inode->pid);
	if (inode->tid == -1 && inode->pid == -1) return (0);
	if ((*thr = tdfind(inode->tid, inode->pid)) == NULL) goto bad;
	//mtx_lock((*thr)->td_lock);
	thread_lock(*thr);
	int test = sleepq_type((*thr)->td_wchan);
	if (!(test != -1 && test & SLEEPQ_CONDVAR)) {
		//mtx_unlock((*thr)->td_lock);
		thread_unlock(*thr);
		goto bad;
	}
	return (0);
bad:
	//CONDFS_PURGE_CONDINODE(inode);
	printf("%s\n", "so bad");
	return (-1);
}
		

static int cdfs_reclaim(struct vop_reclaim_args *va){
	printf("%d %s\n", curthread->td_tid, "Reclaim");
	return (condfs_free_condinode(va->a_vp));
}

static int cdfs_getattr(struct vop_getattr_args *va){
	printf("%d %s\n", curthread->td_tid, "Getattr");
	struct vnode *vp = va->a_vp;
	struct condinode *inode = (struct condinode*)vp->v_data;
	struct vattr *vap = va->a_vap;
	struct thread *thr = NULL;
	if (test_condvar(&thr, inode) != 0) {
	 	if (thr != NULL) mtx_unlock(&(thr->td_proc->p_mtx));
		return (ENOENT);
	}
	vap->va_fileid = inode->tid < 0 ? 2 : inode->tid;
	if (thr != NULL) {
		//mtx_unlock(thr->td_lock);
		thread_unlock(thr);
		mtx_unlock(&(thr->td_proc->p_mtx));
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
	vap->va_mode = 0666; /*I think, I should change it*/
	vap->va_uid = 0;
	vap->va_gid = 0;
	printf("%d %s\n", curthread->td_tid, "out GETATTR");
	return (0);
}

static int cdfs_open(struct vop_open_args *va){
	printf("%d %s\n", curthread->td_tid, "Open");
	return (0);
}

static int cdfs_close(struct vop_close_args *va){
	printf("%d %s\n", curthread->td_tid, "Close");
	/*if (mtx_owned(&root.condvnp_mutex) != 0){
		free(root.condvnp->v_data, M_CDFSN);
		mtx_unlock(&root.condvnp_mutex);
		mtx_unlock(&root.condvnp_mutex);
	}*/
	return (0);
}

static int cdfs_access(struct vop_access_args *va){
	/*I think it is not necessary to call vaccess()*/
	printf("%d %s\n", curthread->td_tid, "Access");
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
		if (mtx_owned(&((*p)->p_mtx)) == 0)
			mtx_lock(&((*p)->p_mtx));
                *t = (*t == NULL) ? TAILQ_FIRST(&((*p)->p_threads)) : TAILQ_NEXT((*t), td_plist);
                while (*t != NULL){
			//mtx_lock((*t)->td_lock);
			thread_lock(*t);
                        int test = sleepq_type((*t)->td_wchan);
			//mtx_unlock((*t)->td_lock);
			thread_unlock(*t);
                        if (test != -1 && test & SLEEPQ_CONDVAR){
                                /*We got condvar!*/
                                return (0);
                        }
                        *t = TAILQ_NEXT((*t), td_plist);
                }
		mtx_unlock(&((*p)->p_mtx));
                *p = LIST_NEXT((*p), p_list);
        }
        return (-1);
}

static int cdfs_fastlookup(struct vop_lookup_args *va){
	struct componentname *cnp = va->a_cnp;
	struct mount *mp;
	int thr, i;
	printf("%d Fastlookup %s %s\n", curthread->td_tid, va->a_gen.a_desc->vdesc_name, cnp->cn_nameptr);
	if (va->a_dvp->v_type != VDIR)
		return (ENOTDIR);
	if ((cnp->cn_flags & ISLASTCN) &&
		(cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EOPNOTSUPP);
	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.'){
		*(va->a_vpp) = va->a_dvp;
		VREF(va->a_dvp);
		printf("%d %s\n", curthread->td_tid, "out LOOKUP it is a .");
		return (0);
	}
	mp = va->a_dvp->v_mount;
	if (cnp->cn_flags & ISDOTDOT){
		return (EIO);
	}
	for (thr = 0, i = 0; i < cnp->cn_namelen && isdigit(cnp->cn_nameptr[i]); ++i)
		thr = thr * 10 + cnp->cn_nameptr[i] - '0';
	if (i != cnp->cn_namelen) return (ENOENT);
	printf("%d fastlookup before iterate\n", curthread->td_tid);
	sx_slock(&allproc_lock);
	struct proc *p = NULL;
	struct thread *t = NULL;
	while(cdfs_iterate(&p, &t) != -1) {
		if (thr == t->td_tid) {
			pid_t pid = p->p_pid;
			lwpid_t tid = t->td_tid;
			mtx_unlock(&(p->p_mtx));
			sx_sunlock(&allproc_lock);
			printf("%d fastlookup find it\n", curthread->td_tid);
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

	/*if (root.condvnp != NULL){
		VI_LOCK(root.condvnp);
		vget(root.condvnp, LK_EXCLUSIVE | LK_INTERLOCK, curthread);
		*(va->a_vpp) = root.condvnp;
		cache_purge(root.condvnp);
	}
	else {
		condfs_alloc_vnode(mp, (va->a_vpp), NULL);
		root.condvnp = *(va->a_vpp);
	}
	if (root.active_condvnp) free((*(va->a_vpp))->v_data, M_CDFSN);
	(*(va->a_vpp))->v_data = (char*)(malloc(sizeof(char) * 10, M_CDFSN, M_WAITOK));
	root.active_condvnp = 1;
	sprintf((*(va->a_vpp))->v_data, "%d", thr);*/
	/*smth loke that*/

	/*teeeEEEMP*///condfs_alloc_vnode(mp, (va->a_vpp), NULL);
	/*(*(va->a_vpp))->v_data = (char*)(malloc(sizeof(char) * 10, M_CDFSN, M_WAITOK));
	sprintf((*(va->a_vpp))->v_data, "%d", thr);*/

	//(*(va->a_vpp))->v_data = cnp->cn_nameptr;
	
	/*Crazy stuff*/
	/*mtx_lock(&root.condvnp_mutex);
	(*(va->a_vpp))->v_data = (int*)(malloc(sizeof(int), M_CDFSN, M_WAITOK));
	*(int*)((*(va->a_vpp))->v_data) = thr;*/

	/*printf("%s\n", "out LOOKUP");
	return (0);*/
}

#ifdef _SLOW_LOOKUP
//static int cdfs_lookup(struct vop_cachedlookup_args *va){
static int cdfs_lookup(struct vop_lookup_args *va){
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
		printf("%s\n", "out LOOKUP it is a .");
		return (0);
	}
	mp = va->a_dvp->v_mount;
	if (cnp->cn_flags & ISDOTDOT){
		printf("%s\n", "out LOOKUP it is a dotdot");
		return (EIO); /*I think so...but I can mistake*/
	}
	/*Check, is it a condvar*/
	for (thr = 0, i = 0; i < cnp->cn_namelen && isdigit(cnp->cn_nameptr[i]); ++i)
		/*I think, it is a very bad comparision. Tid can be any number*/
		//if ((thr = thr * 10 + cnp->cn_nameptr[i] - '0') > PID_MAX + NO_PID)
			//break;
		thr = thr * 10 + cnp->cn_nameptr[i] - '0';
	if (i != cnp->cn_namelen) return (ENOENT);
	sx_slock(&allproc_lock);
	struct proc *p = NULL;
	struct thread *t = NULL;
	while(cdfs_iterate(&p, &t) != -1) {
		if (thr == t->td_tid) {
			mtx_unlock(&(p->p_mtx));
			sx_sunlock(&allproc_lock);
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
				/*
				 * I think, if thr == t->td_tid, that means, 
				 * we has this mutex, so we should unlock it
				 * without mtx_owned() test.
				 * p->p_mtx is unlocked only if cdfs_iterare returns -1
				 */
					//if (mtx_owned(&(p->p_mtx)) != 0)
					//mtx_unlock(&(p->p_mtx));
					//sx_sunlock(&allproc_lock);
					printf("%s\n", "out LOOKUP another dotdot");
					return (ENOENT);
				}
			}
			if (cnp->cn_flags & MAKEENTRY && !(va->a_dvp->v_iflag & VI_DOOMED))
				cache_enter(va->a_dvp, *(va->a_vpp), cnp);
			//if (mtx_owned(&(p->p_mtx)) != 0)
			//mtx_unlock(&(p->p_mtx));
			//sx_sunlock(&allproc_lock);
			(*(va->a_vpp))->v_data = cnp->cn_nameptr;
			printf("%s\n", "out LOOKUP");
			return (0);	
		}
	}
	sx_sunlock(&allproc_lock);
	printf("%s\n", "out LOOKUP enoent");
	return (ENOENT);
}
#endif

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
	printf("%d %s\n", curthread->td_tid, "Readdir");
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
	/*kostyl*/
	int kost = 1;
	for (; offset > 0; offset -= 32){
		if (cdfs_iterate(&p, &t) == -1){
			//mtx_unlock(&(p->p_mtx));
			//sx_sunlock(&allproc_lock);
			kost--;
			offset -= 32;
			if (offset <= 0) break;
			//if (offset > 0) break;
			sx_sunlock(&allproc_lock);
			printf("%d %s\n", curthread->td_tid, "out READDIR nothing to read");
			return (0);
		}
	}

	while (kost == 1 && (cdfs_iterate(&p, &t) != -1 && resid >= 32)){
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
		cdfsent->entry.d_type = DT_REG;
		STAILQ_INSERT_TAIL(&lst, cdfsent, link);
		offset += 32;
		resid -= 32;
	}
	/*temp*/ //if (p != NULL && (mtx_owned(&(p->p_mtx)) != 0)) mtx_unlock(&(p->p_mtx));
	sx_sunlock(&allproc_lock);
	if (kost == 1 && resid >= 32){
		if ((cdfsent = malloc(sizeof(struct cdfsentry), M_IOV,
			M_NOWAIT | M_ZERO)) == NULL)
				return (ENOMEM);
		cdfsent->entry.d_reclen = 32;
		cdfsent->entry.d_fileno = 3;
		char name[3];
		sprintf(name, "%s", "..");
		strcpy(cdfsent->entry.d_name, name);
		cdfsent->entry.d_namlen = strlen(name);
		cdfsent->entry.d_type = DT_DIR;
		STAILQ_INSERT_TAIL(&lst, cdfsent, link);
		offset += 32;
		resid -= 32;
	}
	if (resid >= 32){
		if ((cdfsent = malloc(sizeof(struct cdfsentry), M_IOV,
			M_NOWAIT | M_ZERO)) == NULL)
				return (ENOMEM);
		cdfsent->entry.d_reclen = 32;
		cdfsent->entry.d_fileno = 2;
		char name[2];
		sprintf(name, "%s", ".");
		strcpy(cdfsent->entry.d_name, name);
		cdfsent->entry.d_namlen = strlen(name);
		cdfsent->entry.d_type = DT_DIR;
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
	//sx_sunlock(&allproc_lock); // well, I can do it earlier
	printf("%d %s\n", curthread->td_tid, "out READDIR");
	return (error);
}

static int cdfs_read(struct vop_read_args *va){
	printf("%d %s\n", curthread->td_tid, "Read");
	struct vnode *vp = va->a_vp;
	struct condinode *inode = (struct condinode*)vp->v_data;
	struct sbuf *sb = NULL;
	struct uio *uio = va->a_uio;
	struct thread *thr = NULL;
	int error;
	off_t buflen;
	/*if (test_condvar(&thr, inode) != 0) {
		if (thr != NULL) mtx_unlock(&(thr->td_proc->p_mtx));
		return (ENOENT);
	}*/
	if (uio->uio_resid < 0 || uio->uio_offset < 0 ||
		uio->uio_resid > OFF_MAX - uio->uio_offset) {
		/*mtx_unlock(thr->td_lock);
		mtx_unlock(&(thr->td_proc->p_mtx));*/
		return (EINVAL);
	}
	if (vp->v_type != VREG){
		/*mtx_unlock(thr->td_lock);
		mtx_unlock(&(thr->td_proc->p_mtx));*/
		return (EINVAL);
	}

	buflen = uio->uio_offset + uio->uio_resid;
	if (buflen > MAXPHYS)
		buflen = MAXPHYS;
	//printf("%s\n", "try to create buf");
	sb = sbuf_new(sb, NULL, buflen + 1, 0);
	//printf("%s\n", "feel buf");
	if (test_condvar(&thr, inode) != 0) {
		if (thr != NULL) mtx_unlock(&(thr->td_proc->p_mtx));
		sbuf_delete(sb);
		return (ENOENT);
	}
	sbuf_printf(sb, "%d: %s\n", inode->tid, thr->td_wmesg);
	//sbuf_printf(sb, "%s\n", thr->td_wmesg);
	//printf("%s\n", "unlock");
	//mtx_unlock(thr->td_lock);
	thread_unlock(thr);
	mtx_unlock(&(thr->td_proc->p_mtx));
	printf("%s\n", thr->td_wmesg);
	buflen = sbuf_len(sb);
	//printf("%s\n", "uiomove");
	error = uiomove_frombuf(sbuf_data(sb), buflen, uio);
	//printf("%s\n", "delete");
	sbuf_delete(sb);
	return (error);
}

static int cdfs_write(struct vop_write_args *va){
	printf("%d %s\n", curthread->td_tid, "Write");
	struct vnode *vp = va->a_vp;
	struct condinode *inode = (struct condinode*)vp->v_data;
	struct sbuf sb;
	struct uio *uio = va->a_uio;
	struct thread *thr = NULL;
	int error;
	if (vp->v_type != VREG)
		return (EINVAL);
	/*if (test_condvar(&thr, inode) != 0) {
		if (thr != NULL) mtx_unlock(&(thr->td_proc->p_mtx));
		return (ENOENT);
	}*/
	sbuf_uionew(&sb, uio, &error);
	if (error){
		/*mtx_unlock(thr->td_lock);
		mtx_unlock(&(thr->td_proc->p_mtx));*/
		return (error);
	}
	if (strcmp(sbuf_data(&sb), "signal") == 0){
		if (test_condvar(&thr, inode) != 0) {
			if (thr != NULL) mtx_unlock(&(thr->td_proc->p_mtx));
			sbuf_delete(&sb);
			return (ENOENT);
		}
		cv_signal(thr->td_wchan);
		//mtx_unlock(thr->td_lock);
		thread_unlock(thr);
		mtx_unlock(&(thr->td_proc->p_mtx));
	}
	/*mtx_unlock(thr->td_lock);
	mtx_unlock(&(thr->td_proc->p_mtx));*/
	sbuf_delete(&sb);
	return (0);
}

static int cdfs_setattr(struct vop_setattr_args *va){
	/*I think, it is stupid, but...*/
	printf("%s\n", "Setattr");
	//return (EOPNOTSUPP);
	return (0);
}

/* Remove this function?..*/
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
	.vop_access = cdfs_access,
	.vop_close = cdfs_close,
	//.vop_cachedlookup = cdfs_lookup,
	.vop_cachedlookup = VOP_EOPNOTSUPP,
	.vop_create = VOP_EOPNOTSUPP,
	.vop_getattr = cdfs_getattr,
	.vop_getextattr = VOP_EOPNOTSUPP,
	.vop_ioctl = VOP_EOPNOTSUPP,
	.vop_link = VOP_EOPNOTSUPP,
	//.vop_lookup = vfs_cache_lookup,
	//.vop_lookup = cdfs_lookup,
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
