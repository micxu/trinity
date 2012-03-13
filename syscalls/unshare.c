/*
 * SYSCALL_DEFINE1(unshare, unsigned long, unshare_flags)
 */
#include <linux/sched.h>

#include "trinity.h"
#include "sanitise.h"

struct syscall syscall_unshare = {
	.name = "unshare",
	.num_args = 1,
	.arg1name = "unshare_flags",
	.arg1type = ARG_LIST,
	.arg1list = {
		.num = 7,
		.values = { CLONE_FILES, CLONE_FS, CLONE_NEWIPC, CLONE_NEWNET,
				CLONE_NEWNS, CLONE_SYSVSEM, CLONE_NEWUTS,
		 },
	},
};
