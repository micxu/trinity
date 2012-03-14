/*
 * SYSCALL_DEFINE3(chown16, const char __user *, filename, old_uid_t, user, old_gid_t, group)
 *
 * On success, zero is returned.
 * On error, -1 is returned, and errno is set appropriately.
 */
#include "trinity.h"
#include "sanitise.h"

struct syscall syscall_chown16 = {
	.name = "chown",
	.num_args = 3,
	.arg1name = "filename",
	.arg1type = ARG_ADDRESS,
	.arg2name = "user",
	.arg3name = "group",
	.rettype = RET_ZERO_SUCCESS,
};
