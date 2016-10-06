#ifndef _CONDFS_STRUCTS_H
#define _CONDFS_STRUCTS_H

#include <sys/types.h>

typedef enum {
	cfstype_none = 0,
	cfstype_root,
	cfstype_dir,
	cfstype_file
} cfs_type;

/*struct cfs_node {
	//char c_name[50];
	cfs_type c_type;
	int active_condvnp;
	//int c_flags;
	struct mtx condvnp_mutex;
	struct mtx readdir_mutex;
	//void *c_date;
	struct vnode *vnp;
	//struct vnode *condvnp;
	//struct cfs_node *c_parent;
	//struct cfs_node *c_nodes;
	//struct cfs_node *c_next;
};*/

struct condinode{
	cfs_type type; /*deprecated?*/
	struct vnode *vnp;
	pid_t pid; /*thr->td_proc->p_pid*/
	//struct proc *process; /*thr->td_proc*/
	lwpid_t tid; /*probably deprecated thr->td_tid*/
	//struct thread *thr;
	struct condinode *next;
	struct condinode *prev;
};

#endif
