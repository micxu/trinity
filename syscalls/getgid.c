/*
 * SYSCALL_DEFINE0(getgid)
 */
#include "trinity.h"
#include "sanitise.h"

struct syscall syscall_getgid = {
	.name = "getgid",
	.num_args = 0,
	.rettype = RET_GID_T,
};
