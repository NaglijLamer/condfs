#ifndef _CONDFS_STRUCTS_H
#define _CONDFS_STRUCTS_H

typedef enum {
	cfstype_none = 0,
	cfstype_root,
	cfstype_dir,
	cfstype_this,
	cfstype_parent,
	cfstype_file
} cfs_type_t;

struct cfs_node {
	char c_name[50];
	cfs_type_t c_type;
	int c_flags;
	struct mtx c_mutex;
	void *c_date;
	struct vnode *vnp;
	struct cfs_node *c_parent;
	struct cfs_node *c_nodes;
	struct cfs_node *c_next;
};

#endif
