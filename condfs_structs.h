#ifndef _CONDFS_STRUCTS_H
#define _CONDFS_STRUCTS_H

#include <sys/systm.h>

#ifdef _DEBUG_CONDFS
#define PRINTF_DEBUG(format, ...) printf(format, __VA_ARGS__)
#else
#define PRINTF_DEBUG(...)
#endif

typedef enum {
	cfstype_none = 0,
	cfstype_root,
	cfstype_dir,
	cfstype_file
} cfs_type;

struct condinode{
	cfs_type type; /*deprecated?*/
	struct vnode *vnp;
	pid_t pid; /*thr->td_proc->p_pid*/
	lwpid_t tid; /*probably deprecated thr->td_tid*/
	struct condinode *next;
	struct condinode *prev;
};

#endif
