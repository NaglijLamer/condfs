#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>

static int cevent_handler(struct module *module, int event, void *arg){
	int error = 0;
	switch (event){
		case MOD_LOAD:
			printf("%s\n", "HOLY SHIIIIIII");
			break;
		case MOD_UNLOAD:
			printf("%s\n", "LOLE BYE");
			break;
		default:
			error = EOPNOTSUPP;
			break;
	}
	return (error);
}

static moduledata_t condfs_conf = {
	"condfs",
	cevent_handler,
	NULL,
};

DECLARE_MODULE(condfs, condfs_conf, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
