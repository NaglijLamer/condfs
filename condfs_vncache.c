#include <sys/ctype.h>
#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/eventhandler.h>
#include "condfs_vncache.h"
#include "condfs_structs.h"

extern struct condinode *rooti;
extern struct vop_vector cfs_vnodeops;
struct condinode *taili;
static eventhandler_tag condfs_exit_tag;
struct mtx cache_mutex;
static const char *condfs_name = "condfs";

static MALLOC_DEFINE (M_CDFSCONDI, "lole", "debuglole");

/*Create new node in linked list rooti*/
int create_condinode(struct condinode **inode, cfs_type type, struct vnode *vnp, lwpid_t tid, pid_t pid){
	//struct condinode *inode = *pinode;
	mtx_lock(&cache_mutex);
	if ((type == cfstype_root && (pid != -1 || tid != -1)) ||
		(taili == NULL && type != cfstype_root))
			CDFS_CACHE_RET(ENOSYS);
	/*if ((type == cfstype_root && (taili != NULL || tid != -1)) ||
		(type != cfstype_root && (taili == NULL || tid < 0)))
			CDFS_CACHE_RET(ENOSYS);*/
	*inode = (struct condinode*)malloc(sizeof(struct condinode), M_CDFSCONDI, M_WAITOK);
	/*if (*inode == NULL) 
		CDFS_CACHE_RET(ENOMEM);*/ 
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
 * If there is no vnode with this inode in cache 
 * then we should get new vnode from free list
 */
//int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, struct condinode *inode){
int condfs_alloc_vnode(struct mount *mp, struct vnode **vpp, pid_t pid, lwpid_t tid){
	struct condinode *nexti;
	struct vnode *vnp;
	int error;
retry:
	mtx_lock(&cache_mutex);
	/*Check cache*/
	for (nexti = taili; nexti; nexti = nexti->next){
		if (nexti->tid == tid && nexti->vnp->v_mount == mp){
			if (nexti->pid != pid) nexti->pid = pid;
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
	printf("%s\n", "Cant find");
	//mtx_unlock(&cache_mutex); or we should check with retry2 see pseudofs
	/*vnode not found*/
	error = getnewvnode(condfs_name, mp, &cfs_vnodeops, vpp);
	printf("%s\n", "Get this shit");
	if (error) return (error);
	if (tid < 0) (*vpp)->v_vflag = VV_ROOT;
	(*vpp)->v_type = tid >= 0 ? VREG : VDIR;
	printf("%s\n", "Try to create inode");
	create_condinode(&nexti, tid >= 0 ? cfstype_file : cfstype_root, *vpp, tid, pid);
	printf("%s\n", "create inode shit");
	(*vpp)->v_data = nexti;
	if (nexti == NULL) printf("%s\n" , "problemes without ends");
	if ((*vpp)->v_data == NULL) printf("%s\n", "soooproblemes");
	if (taili->vnp->v_data == NULL) printf("%s\n", "problemes");
	error = insmntque(*vpp, mp);
	if (error != 0) {
		taili = nexti->next;
		taili->prev = NULL;
		free(nexti, M_CDFSCONDI);
		*vpp = NULLVP;
		mtx_unlock(&cache_mutex);
		return (error);
	}
	//mtx_unlock(&cache_mutex);
	printf("%s\n", "out this shit");
	CDFS_CACHE_RET(0);
}

void condfs_purge(struct condinode *inode, pid_t pid){
	struct condinode *nexti;
	struct vnode *vnp;
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
		}
		else nexti = nexti->next;
	}
	mtx_unlock(&cache_mutex);
}

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
		
int condfs_free_condinode(struct vnode *vp){
	struct condinode *inode;
	printf("%s\n", "Enter");
	mtx_lock(&cache_mutex);
	inode = (struct condinode*)(vp->v_data);
	if (inode == NULL) printf("%s %s\n", "vbad", vp->v_tag);
	printf("%s %d\n", "next", ((struct condinode*)(taili->vnp->v_data))->tid);
	if (inode->next)
		inode->next->prev = inode->prev;
	printf("%s\n", "prev");
	if (inode->prev)
		inode->prev->next = inode->next;
	else 
		taili = inode->next;
	printf("%s\n", "unlock");
	mtx_unlock(&cache_mutex);
	printf("%s\n", "freeeeee");
	free(inode, M_CDFSCONDI);
	printf("%s\n", "ret");
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
