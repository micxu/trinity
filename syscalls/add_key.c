/*
 * SYSCALL_DEFINE5(add_key, const char __user *, _type,
	const char __user *, _description,
	const void __user *, _payload,
	size_t, plen,
	key_serial_t, ringid)
 *
 * On success add_key() returns the serial number of the key it created or updated.
 * On error, the value -1 will be returned and errno will have been set to an appropriate error.
 */
#include "random.h"
#include "sanitise.h"

static const char *keytypes[] = {
	"user", "keyring", "big_key",
};

static void sanitise_add_key(struct syscallrecord *rec)
{
	rec->a1 = (unsigned long) RAND_ARRAY(keytypes);
}

struct syscallentry syscall_add_key = {
	.name = "add_key",
	.num_args = 5,
	.arg1name = "_type",
	.arg1type = ARG_ADDRESS,
	.arg2name = "_description",
	.arg2type = ARG_ADDRESS,
	.arg3name = "_payload",
	.arg3type = ARG_ADDRESS,
	.arg4name = "plen",
	.arg4type = ARG_LEN,
	.arg5name = "ringid",
	.rettype = RET_KEY_SERIAL_T,
	.sanitise = sanitise_add_key,
};
