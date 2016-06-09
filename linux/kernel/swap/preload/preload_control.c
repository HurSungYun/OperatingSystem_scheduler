#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/limits.h>

#include <us_manager/sspt/ip.h>

#include "preload.h"

#include "preload_control.h"
#include "preload_probe.h"
#include "preload_module.h"


#define DEFAULT_SLOTS_COUNT 5
#define DEFAULT_SLOTS_STEP 2

struct bin_desc {
	struct dentry *dentry;
	char *filename;
};

static struct bin_desc *target_binaries = NULL;
static unsigned int target_binaries_cnt = 0;
static unsigned int target_binaries_slots = 0;

static DEFINE_MUTEX(__target_binaries_mutex);


static inline void __target_binaries_lock(void)
{
	mutex_lock(&__target_binaries_mutex);
}

static inline void __target_binaries_unlock(void)
{
	mutex_unlock(&__target_binaries_mutex);
}

static inline struct task_struct *__get_task_struct(void)
{
	return current;
}

static int __alloc_target_binaries_no_lock(unsigned int cnt)
{
	target_binaries = kmalloc(sizeof(*target_binaries) * cnt, GFP_KERNEL);
	if (target_binaries == NULL)
		return -ENOMEM;

	target_binaries_slots = cnt;

	return 0;
}

static int __alloc_target_binaries(unsigned int cnt)
{
	int ret;

	__target_binaries_lock();
	ret = __alloc_target_binaries_no_lock(cnt);
	__target_binaries_unlock();

	return ret;
}

static void __free_target_binaries(void)
{
	int i;

	__target_binaries_lock();

	for (i = 0; i < target_binaries_cnt; i++) {
		put_dentry(target_binaries[i].dentry);
		kfree(target_binaries[i].filename);
	}

	kfree(target_binaries);
	target_binaries_cnt = 0;
	target_binaries_slots = 0;

	__target_binaries_unlock();
}

static int __grow_target_binaries(void)
{
	struct bin_desc *tmp = target_binaries;
	int i, ret;

	__target_binaries_lock();

	ret = __alloc_target_binaries_no_lock(target_binaries_slots + DEFAULT_SLOTS_STEP);
	if (ret != 0)
		return ret;

	target_binaries_slots += DEFAULT_SLOTS_STEP;

	for (i = 0; i < target_binaries_cnt; i++) {
		target_binaries[i].dentry = tmp[i].dentry;
		target_binaries[i].filename = tmp[i].filename;
	}

	__target_binaries_unlock();

	kfree(tmp);

	return 0;
}

static bool __check_dentry_already_exist(struct dentry *dentry)
{
	int i;
	bool ret = false;

	__target_binaries_lock();

	for (i = 0; i < target_binaries_cnt; i++) {
		if (target_binaries[i].dentry == dentry) {
			ret = true;
			goto check_dentry_unlock;
		}
	}

check_dentry_unlock:
	__target_binaries_unlock();

	return ret;
}

static int __add_target_binary(struct dentry *dentry, char *filename)
{
	int ret;
	size_t len;

	if (__check_dentry_already_exist(dentry)) {
		printk(PRELOAD_PREFIX "Binary already exist\n");
		return EALREADY;
	}


	if (target_binaries_slots == target_binaries_cnt) {
		ret = __grow_target_binaries();
		if (ret != 0)
			return ret;
	}

	/* Filename should be < PATH_MAX */
	len = strnlen(filename, PATH_MAX);
	if (len == PATH_MAX)
		return -EINVAL;

	__target_binaries_lock();

	target_binaries[target_binaries_cnt].dentry = dentry;
	target_binaries[target_binaries_cnt].filename = kmalloc(len + 1, GFP_KERNEL);
	memcpy(target_binaries[target_binaries_cnt].filename, filename, len + 1);
	++target_binaries_cnt;

	__target_binaries_unlock();

	return 0;
}

static char *__get_binary_name(struct bin_desc *bin)
{
	return bin->filename;
}

static struct dentry *__get_caller_dentry(struct task_struct *task,
					  unsigned long caller)
{
	struct vm_area_struct *vma = NULL;

	if (unlikely(task->mm == NULL))
		goto get_caller_dentry_fail;

	vma = find_vma_intersection(task->mm, caller, caller + 1);
	if (unlikely(vma == NULL || vma->vm_file == NULL))
		goto get_caller_dentry_fail;

	return vma->vm_file->f_dentry;

get_caller_dentry_fail:

	return NULL;
}

static bool __check_if_instrumented(struct task_struct *task,
				    struct dentry *dentry)
{
	int i;

	for (i = 0; i < target_binaries_cnt; i++)
		if (target_binaries[i].dentry == dentry)
			return true;

	return false;
}

static bool __is_instrumented(void *caller)
{
	struct task_struct *task = __get_task_struct();
	struct dentry *caller_dentry = __get_caller_dentry(task,
							   (unsigned long) caller);

	if (caller_dentry == NULL)
		return false;

	return __check_if_instrumented(task, caller_dentry);
}


/* Called only form handlers. If we're there, then it is instrumented. */
enum preload_call_type preload_control_call_type_always_inst(void *caller)
{
	if (__is_instrumented(caller))
		return INTERNAL_CALL;

	return EXTERNAL_CALL;

}

enum preload_call_type preload_control_call_type(struct us_ip *ip, void *caller)
{
	if (__is_instrumented(caller))
		return INTERNAL_CALL;

	if (ip->info->pl_i.flags & SWAP_PRELOAD_ALWAYS_RUN)
		return EXTERNAL_CALL;

	return NOT_INSTRUMENTED;
}

int preload_control_add_instrumented_binary(char *filename)
{
	struct dentry *dentry = get_dentry(filename);
	int res = 0;

	if (dentry == NULL)
		return -EINVAL;

	res = __add_target_binary(dentry, filename);
	if (res != 0)
		put_dentry(dentry);

	return res > 0 ? 0 : res;
}

int preload_control_clean_instrumented_bins(void)
{
	__free_target_binaries();
	return __alloc_target_binaries(DEFAULT_SLOTS_COUNT);
}

unsigned int preload_control_get_bin_names(char ***filenames_p)
{
	int i;
	unsigned int ret = 0;

	if (target_binaries_cnt == 0)
		return 0;

	__target_binaries_lock();

	*filenames_p = kmalloc(sizeof(**filenames_p) * target_binaries_cnt,
			   GFP_KERNEL);
	if (*filenames_p == NULL)
		goto get_binaries_names_out;

	for (i = 0; i < target_binaries_cnt; i++)
		(*filenames_p)[i] = __get_binary_name(&target_binaries[i]);

	ret = target_binaries_cnt;

get_binaries_names_out:
	__target_binaries_unlock();

	return ret;
}

void preload_control_release_bin_names(char ***filenames_p)
{
	kfree(*filenames_p);
}

int preload_control_init(void)
{
	return __alloc_target_binaries(DEFAULT_SLOTS_COUNT);
}

void preload_control_exit(void)
{
	__free_target_binaries();
}

#undef DEFAULT_SLOTS_STEP
#undef DEFAULT_SLOTS_COUNT
