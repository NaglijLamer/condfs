#ifndef _CONDFS_VNOPS_H
#define _CONDFS_VNOPS_H

#define MAGIC_SIZE 32

#

#define ADD_DIRECTORY_CDFSENTRY(fileno, type, format, name) \
	do { \
		if ((cdfsent = malloc(sizeof(struct cdfsentry), M_IOV, \
			M_NOWAIT | M_ZERO)) == NULL) \
				return (ENOMEM); \
		cdfsent->entry.d_reclen = MAGIC_SIZE; \
		cdfsent->entry.d_fileno = fileno; \
		sprintf(cdfsent->entry.d_name, format, name); \
		cdfsent->entry.d_type = type; \
		cdfsent->entry.d_namlen = strlen(cdfsent->entry.d_name); \
		STAILQ_INSERT_TAIL(&lst, cdfsent, link); \
		offset += MAGIC_SIZE; \
		resid -= MAGIC_SIZE; \
	} while (0)
/*char namec[100];
                if (type == DT_REG)
                        sprintf(namec, "%d", name t->td_tid);
                else
                        sprintf(namec, "%s", name);
                strcpy(cdfsent->entry.d_name, namec);*/

inline int test_condvar(struct thread **thr, struct condinode *inode);

#endif
