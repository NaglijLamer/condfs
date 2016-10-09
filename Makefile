KMOD = condfs
SRCS = condfs.c condfs_vnops.c condfs_vncache.c vnode_if.h
.if !empty(NFLAGS)
CFLAGS += -D_DEBUG_CONDFS
.endif

.include <bsd.kmod.mk>
