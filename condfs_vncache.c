#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/eventhandler.h>
#include "condfs_structs.h"
#include "condfs_vncache.h"

extern struct vop_vector cfs_vnodeops;
struct condinode *taili;
static eventhandler_tag condfs_exit_tag;
struct mtx cache_mutex;
static const char *condfs_name = "condfs";

static MALLOC_DEFINE (M_CDFSCONDI, "lole", "debuglole");

/*
 * Create new node in linked list taili
 */
int create_condinode(struct condinode **inode, cfs_type type, struct vnode *vnp, lwpid_t tid, pid_t pid){
	mtx_lock(&cache_mutex);
	if ((type == cfstype_root && (pid != -1 || tid != -1)) ||
		(taili == NULL && type != cfstype_root))
			CDFS_CACHE_RET(ENOSYS);
	*inode = (struct condinode*)malloc(sizeof(struct condinode), M_CDFSCONDI, M_WAITOK);
	//Cannot return NULL, cause we use M_WAITOK
	(*inode)->type = type;
	(*inode)->vnp = vnp;
	(*inode)->tid = tid;
	(*inode)->pid = pid;
	
	(*inode)->next = taili;
	if (taili != NULL) taili->prev = *inode;
	(*inode)->prev = NULL;
	taili = *inode;

	CDFS_CACHE_RET(0);
}

/*
 * Get vnode for condinode.
 * First of all we check our cache.
 * If there is no vnode with this condinode in cache 
 * then we should get new vnode from free list
 */
int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, pid_t pid, lwpid_t tid){
	struct condinode *nexti;
	struct vnode *vnp;
	int error;
retry:
	mtx_lock(&cache_mutex);
	//Check cache
	for (nexti = taili; nexti; nexti = nexti->next){
		if (nexti->tid == tid && nexti->vnp->v_mount == mp){
			if (nexti->pid != pid) nexti->pid = pid; /*continue;*/
			vnp = nexti->vnp;
			VI_LOCK(vnp);
			mtx_unlock(&cache_mutex);
			if (vget(vnp, LK_EXCLUSIVE | LK_INTERLOCK, curthread) == 0) {
				*vpp = vnp;
				return (0);
			}
			goto retry;
		}
	}
	//vnode not found
	if ((error = getnewvnode(condfs_name, mp, &cfs_vnodeops, vpp))) 
		CDFS_CACHE_RET(error);
	if (tid < 0) (*vpp)->v_vflag = VV_ROOT;
	(*vpp)->v_type = tid >= 0 ? VREG : VDIR;
	if ((error = create_condinode(&nexti, tid >= 0 ? cfstype_file : cfstype_root,
		 *vpp, tid, pid))) CDFS_CACHE_RET(error);
	(*vpp)->v_data = nexti;
	error = insmntque(*vpp, mp);
	if (error != 0) {
		taili = nexti->next;
		taili->prev = NULL;
		free(nexti, M_CDFSCONDI);
		*vpp = NULLVP;
		CDFS_CACHE_RET(error);
	}
	CDFS_CACHE_RET(0);
}

/*
 * Free one vnode or all vnodes which condinode->pid == pid
 */
void condfs_purge(struct condinode *inode, pid_t pid){
	struct condinode *nexti;
	struct vnode *vnp;
	PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Purge");
	mtx_lock(&cache_mutex);
	nexti = taili;
	while (nexti != NULL){
		if (nexti == inode || pid == nexti->pid){
			vnp = nexti->vnp;
			vhold(vnp);
			mtx_unlock(&cache_mutex);
			VOP_LOCK(vnp, LK_EXCLUSIVE);
			vgone(vnp);
			VOP_UNLOCK(vnp, 0);
			mtx_lock(&cache_mutex);
			vdrop(vnp);
			nexti = taili;
			PRINTF_DEBUG("%d %s\n", curthread->td_tid, "Purge success");
		}
		else nexti = nexti->next;
	}
	mtx_unlock(&cache_mutex);
}

/*
 * Free all vnodes
 */
void condfs_purge_all(){
	struct condinode *nexti;
	mtx_lock(&cache_mutex);
	nexti = taili;
	for (nexti = taili; nexti; nexti = taili){
		taili = nexti->next;
		CONDFS_PURGE_CONDINODE(nexti);
	}
	mtx_unlock(&cache_mutex);
}

/*
 * Remove condinode from cache
 */
int condfs_free_condinode(struct vnode *vp){
	struct condinode *inode;
	mtx_lock(&cache_mutex);
	inode = (struct condinode*)(vp->v_data);
	if (inode->next)
		inode->next->prev = inode->prev;
	if (inode->prev)
		inode->prev->next = inode->next;
	else 
		taili = inode->next;
	mtx_unlock(&cache_mutex);
	free(inode, M_CDFSCONDI);
	vp->v_data = NULL;
	return (0);
}

/*
 * Callback for event of every dying proccess.
 * We should check if we has condinodes for this process
 */
static void condfs_proccexit(void *arg, struct proc *p){
	CONDFS_PURGE_PID(p->p_pid);
}

/*
 * Init/uninit functions for cache
 */
void condfs_vncache_init(void){
	taili = NULL;
	mtx_init(&cache_mutex, "condfs cache mutex", NULL, MTX_DEF);
	condfs_exit_tag = EVENTHANDLER_REGISTER(process_exit, condfs_proccexit, NULL,
		EVENTHANDLER_PRI_ANY);
}

void condfs_vncache_uninit(void){
	EVENTHANDLER_DEREGISTER(process_exit, condfs_exit_tag);
	mtx_destroy(&cache_mutex);
}
