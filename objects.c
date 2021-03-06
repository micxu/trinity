#include "list.h"
#include "log.h"
#include "objects.h"
#include "random.h"
#include "shm.h"
#include "trinity.h"
#include "utils.h"

void dump_objects(bool global, enum objecttype type)
{
	struct list_head *node, *list;
	struct objhead *head;

	head = get_objhead(global, type);
	list = head->list;

	// TODO: objhead->name
	output(0, "There are %d entries in the %d list (@%p).\n",
			head->num_entries, type, list);

	list_for_each(node, list) {
		struct object *obj;
		struct map *m;
		char buf[11];

		obj = (struct object *) node;

		//TODO: Having object.c have knowledge of each object type is kinda
		// gross. Have some kind of ->dump operation in the objhead maybe?

		switch (type) {
		case OBJ_MMAP_ANON:
		case OBJ_MMAP_FILE:
		case OBJ_MMAP_TESTFILE:
			m = &obj->map;
			sizeunit(m->size, buf);
			output(0, " start: %p size:%s  name: %s\n", m->ptr, buf, m->name);
			break;
		case OBJ_FD_PIPE:
			output(0, "pipefd:%d\n", obj->pipefd);
			break;
		case OBJ_FD_FILE:
			output(0, "filefd:%d\n", obj->filefd);
			break;
		case OBJ_FD_PERF:
			output(0, "perffd:%d\n", obj->perffd);
			break;
		case OBJ_FD_EPOLL:
			output(0, "epollfd:%d\n", obj->epollfd);
			break;
		case OBJ_FD_EVENTFD:
			output(0, "eventfd:%d\n", obj->eventfd);
			break;
		case OBJ_FD_TIMERFD:
			output(0, "timerfd:%d\n", obj->timerfd);
			break;
		case OBJ_FD_TESTFILE:
			output(0, "testfilefd:%d\n", obj->testfilefd);
			break;
		case OBJ_FD_MEMFD:
			output(0, "memfd:%d\n", obj->memfd);
			break;
		case OBJ_FD_DRM:
			output(0, "drmfd:%d\n", obj->drmfd);
			break;
		case OBJ_FD_INOTIFY:
			output(0, "inotifyfd:%d\n", obj->inotifyfd);
			break;
		case OBJ_FD_SOCKET:
			output(0, "socket (fam:%d type:%d protocol:%d) fd:%d\n",
				obj->sockinfo.triplet.family,
				obj->sockinfo.triplet.type,
				obj->sockinfo.triplet.protocol,
				obj->sockinfo.fd);
			break;
		case OBJ_FD_USERFAULTFD:
			output(0, "userfaultfd:%d\n", obj->userfaultfd);
			break;
		case OBJ_FD_FANOTIFY:
			output(0, "fanotify:%d\n", obj->fanotifyfd);\
			break;
		case OBJ_FD_BPF_MAP:
			output(0, "bpf map fd:%d\n", obj->bpf_map_fd);
			break;
		case OBJ_FD_BPF_PROG:
			output(0, "bpf prog fd:%d\n", obj->bpf_prog_fd);
			break;
		case OBJ_FUTEX:
			output(0, "futex: %lx owner:%d\n",
				obj->lock.futex, obj->lock.owner_pid);
			break;
		case OBJ_SYSV_SHM:
			output(0, "sysv_shm: id:%u size:%d flags:%x ptr:%p\n",
				obj->sysv_shm.id, obj->sysv_shm.size,
				obj->sysv_shm.flags, obj->sysv_shm.ptr);
			break;
		case MAX_OBJECT_TYPES:
		default:
			break;
		}
	}
}


struct object * alloc_object(void)
{
	struct object *obj;
	obj = zmalloc(sizeof(struct object));
	INIT_LIST_HEAD(&obj->list);
	return obj;
}

struct objhead * get_objhead(bool global, enum objecttype type)
{
	struct objhead *head;

	if (global == OBJ_GLOBAL)
		head = &shm->global_objects[type];
	else {
		struct childdata *child;

		child = this_child();
		head = &child->objects[type];
	}
	return head;
}


void add_object(struct object *obj, bool global, enum objecttype type)
{
	struct objhead *head;

	head = get_objhead(global, type);
	if (head->list == NULL) {
		head->list = zmalloc(sizeof(struct object));
		INIT_LIST_HEAD(head->list);
	}

	list_add_tail(&obj->list, head->list);
	head->num_entries++;

	/* if we just added something to a child list, check
	 * to see if we need to do some pruning.
	 */
	if (global == OBJ_LOCAL)
		prune_objects();
}

void init_object_lists(bool global)
{
	unsigned int i;

	for (i = 0; i < MAX_OBJECT_TYPES; i++) {
		struct objhead *head;

		head = get_objhead(global, i);

		head->list = NULL;
		head->num_entries = 0;

		/*
		 * child lists can inherit properties from global lists.
		 */
		if (global == OBJ_LOCAL) {
			struct objhead *globalhead;
			globalhead = get_objhead(OBJ_GLOBAL, i);
			head->max_entries = globalhead->max_entries;
			head->destroy = globalhead->destroy;
		}
	}
}

struct object * get_random_object(enum objecttype type, bool global)
{
	struct objhead *head;
	struct list_head *node, *list;
	unsigned int i, j = 0, n;

	head = get_objhead(global, type);

	list = head->list;

	n = head->num_entries;
	if (n == 0)
		return NULL;
	i = rnd() % n;

	list_for_each(node, list) {
		struct object *m;

		m = (struct object *) node;

		if (i == j)
			return m;
		j++;
	}
	return NULL;
}

bool objects_empty(enum objecttype type)
{
	return shm->global_objects[type].num_entries == 0;
}

/*
 * Call the destructor for this object, and then release it.
 */
void destroy_object(struct object *obj, bool global, enum objecttype type)
{
	struct objhead *head;

	list_del(&obj->list);

	head = get_objhead(global, type);
	head->num_entries--;

	if (head->destroy != NULL)
		head->destroy(obj);

	free(obj);
}

/*
 * Destroy a whole list of objects.
 */
static void destroy_objects(enum objecttype type, bool global)
{
	struct list_head *node, *list, *tmp;
	struct objhead *head;

	if (objects_empty(type) == TRUE)
		return;

	head = get_objhead(global, type);
	list = head->list;

	list_for_each_safe(node, tmp, list) {
		struct object *obj;

		obj = (struct object *) node;

		destroy_object(obj, global, type);
	}

	head->num_entries = 0;
}

/* Destroy all the global objects.
 *
 * We close this before quitting. All OBJ_LOCAL's got destroyed
 * when the children exited, leaving just these OBJ_GLOBALs
 * to clean up.
 */
void destroy_global_objects(void)
{
	unsigned int i;

	for (i = 0; i < MAX_OBJECT_TYPES; i++)
		destroy_objects(i, OBJ_GLOBAL);
}

/*
 * Think of this as a poor mans garbage collector, to prevent
 * us from exhausting all the available fd's in the system etc.
 */
static void __prune_objects(enum objecttype type, bool global)
{
	struct objhead *head;
	unsigned int num_to_prune;

	if (RAND_BOOL())
		return;

	head = get_objhead(global, type);

	/* 0 = don't ever prune. */
	if (head->max_entries == 0)
		return;

	/* only prune full lists. */
	if (head->num_entries < head->max_entries)
		return;

	num_to_prune = rnd() % head->num_entries;

	while (num_to_prune > 0) {
		struct list_head *node, *list, *tmp;

		list = head->list;

		list_for_each_safe(node, tmp, list) {
			if (ONE_IN(10)) {
				struct object *obj;

				obj = (struct object *) node;
				destroy_object(obj, global, type);
				num_to_prune--;
				//TODO: log something
			}
		}
	}
}

void prune_objects(void)
{
	unsigned int i;

	/* We don't want to over-prune things and growing a little
	 * bit past the ->max is fine, we'll clean it up next time.
	 */
	if (!(ONE_IN(10)))
		return;

	for (i = 0; i < MAX_OBJECT_TYPES; i++) {
		__prune_objects(i, OBJ_LOCAL);
		// For now, we're only pruning local objects.
		// __prune_objects(i, OBJ_GLOBAL);
	}
}
