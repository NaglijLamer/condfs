KMOD = condfs
SRCS = /root/lab1/condfs/condfs.c /root/lab1/condfs/condfs_vnops.c vnode_if.h

.include <bsd.kmod.mk>
