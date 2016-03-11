#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/limits.h>
#include <asm/uaccess.h>
#include <master/swap_debugfs.h>
#include "preload.h"
#include "preload_debugfs.h"
#include "preload_module.h"
#include "preload_control.h"
#include "preload_storage.h"

static const char PRELOAD_FOLDER[] = "preload";
static const char PRELOAD_LOADER[] = "loader";
static const char PRELOAD_LOADER_OFFSET[] = "loader_offset";
static const char PRELOAD_LOADER_PATH[] = "loader_path";
static const char PRELOAD_BINARIES[] = "target_binaries";
static const char PRELOAD_BINARIES_LIST[] = "bins_list";
static const char PRELOAD_BINARIES_ADD[] = "bins_add";
static const char PRELOAD_BINARIES_REMOVE[] = "bins_remove";
static const char PRELOAD_CALLER[] = "caller";
static const char PRELOAD_HANDLERS_PATH[] = "handlers_path";
static const char PRELOAD_LINKER_DATA[] = "linker";
static const char PRELOAD_LINKER_PATH[] = "linker_path";
static const char PRELOAD_LINKER_R_DEBUG_OFFSET[] = "r_debug_offset";

struct loader_info {
	char *path;
	unsigned long offset;
	struct dentry *dentry;
};

static struct dentry *preload_root;
static struct loader_info __loader_info;

static unsigned long r_debug_offset = 0;
static DEFINE_SPINLOCK(__dentry_lock);

static inline void dentry_lock(void)
{
	spin_lock(&__dentry_lock);
}

static inline void dentry_unlock(void)
{
	spin_unlock(&__dentry_lock);
}


static void set_loader_file(char *path)
{
	__loader_info.path = path;
	dentry_lock();
	__loader_info.dentry = get_dentry(__loader_info.path);
	dentry_unlock();
}

struct dentry *preload_debugfs_get_loader_dentry(void)
{
	struct dentry *dentry;

	dentry_lock();
	dentry = __loader_info.dentry;
	dentry_unlock();

	return dentry;
}

unsigned long preload_debugfs_get_loader_offset(void)
{
	/* TODO Think about sync */
	return __loader_info.offset;
}

static void clean_loader_info(void)
{
	if (__loader_info.path != NULL)
		kfree(__loader_info.path);
	__loader_info.path = NULL;

	dentry_lock();
	if (__loader_info.dentry != NULL)
		put_dentry(__loader_info.dentry);

	__loader_info.dentry = NULL;
	__loader_info.offset = 0;

	dentry_unlock();
}

struct dentry *debugfs_create_ptr(const char *name, mode_t mode,
				  struct dentry *parent,
				  unsigned long *value)
{
	struct dentry *dentry;

#if BITS_PER_LONG == 32
	dentry = debugfs_create_x32(name, mode, parent, (u32 *)value);
#elif BITS_PER_LONG == 64
	dentry = debugfs_create_x64(name, mode, parent, (u64 *)value);
#else
#error Unsupported BITS_PER_LONG value
#endif

	return dentry;
}


/* ===========================================================================
 * =                              LOADER PATH                                =
 * ===========================================================================
 */

static ssize_t loader_path_write(struct file *file, const char __user *buf,
				 size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	if (preload_module_is_running())
		return -EBUSY;

	clean_loader_info();

	path = kmalloc(len, GFP_KERNEL);
	if (path == NULL) {
		return -ENOMEM;
	}

	if (copy_from_user(path, buf, len)) {
		ret = -EINVAL;
		goto err;
	}

	path[len - 1] = '\0';
	set_loader_file(path);
	ret = len;

	return ret;
err:
	kfree(path);
	return ret;
}


static const struct file_operations loader_path_file_ops = {
	.owner = THIS_MODULE,
	.write = loader_path_write,
};


/* ===========================================================================
 * =                                BIN PATH                                 =
 * ===========================================================================
 */

static ssize_t bin_add_write(struct file *file, const char __user *buf,
			   size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (path == NULL) {
		ret = -ENOMEM;
		goto bin_add_write_out;
	}

	if (copy_from_user(path, buf, len)) {
		ret = -EINVAL;
		goto bin_add_write_out;
	}

	path[len - 1] = '\0';

	if (preload_control_add_instrumented_binary(path) != 0) {
		printk(PRELOAD_PREFIX "Cannot add binary %s\n", path);
		ret = -EINVAL;
		goto bin_add_write_out;
	}

	ret = len;

bin_add_write_out:
	kfree(path);

	return ret;
}

static ssize_t bin_remove_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	ssize_t ret;

	ret = preload_control_clean_instrumented_bins();
	if (ret != 0) {
		printk(PRELOAD_PREFIX "Error during clean!\n");
		ret = -EINVAL;
		goto bin_remove_write_out;
	}

	ret = len;

bin_remove_write_out:
	return ret;
}

static ssize_t bin_list_read(struct file *file, char __user *usr_buf,
				 size_t count, loff_t *ppos)
{
	unsigned int i;
	unsigned int files_cnt = 0;
	ssize_t len = 0, tmp, ret = 0;
	char **filenames = NULL;
	char *buf = NULL;
	char *ptr = NULL;

	files_cnt = preload_control_get_bin_names(&filenames);

	if (files_cnt == 0) {
		printk(PRELOAD_PREFIX "Cannot read binaries names!\n");
		ret = 0;
		goto bin_list_read_out;
	}

	for (i = 0; i < files_cnt; i++)
		len += strlen(filenames[i]);

	buf = kmalloc(len + files_cnt, GFP_KERNEL);
	if (buf == NULL) {
		ret = 0;
		goto bin_list_read_fail;
	}

	ptr = buf;

	for (i = 0; i < files_cnt; i++) {
		tmp = strlen(filenames[i]);
		memcpy(ptr, filenames[i], tmp);
		ptr += tmp;
		*ptr = '\n';
		ptr += 1;
	}

	preload_control_release_bin_names(&filenames);

	return simple_read_from_buffer(usr_buf, count, ppos, buf, len);

bin_list_read_fail:
	preload_control_release_bin_names(&filenames);

bin_list_read_out:
	return ret;
}

static const struct file_operations bin_list_file_ops = {
	.owner = THIS_MODULE,
	.read = bin_list_read
};

static const struct file_operations bin_add_file_ops = {
	.owner = THIS_MODULE,
	.write = bin_add_write,
};

static const struct file_operations bin_remove_file_ops = {
	.owner = THIS_MODULE,
	.write = bin_remove_write,
};


/* ===========================================================================
 * =                            LINKER PATH                                  =
 * ===========================================================================
 */


static ssize_t linker_path_write(struct file *file, const char __user *buf,
				  size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (path == NULL) {
		ret = -ENOMEM;
		goto linker_path_write_out;
	}

	if (copy_from_user(path, buf, len)) {
		ret = -EINVAL;
		goto linker_path_write_out;
	}

	path[len - 1] = '\0';

	if (preload_storage_set_linker_info(path) != 0) {
		printk(PRELOAD_PREFIX "Cannot set linker path %s\n", path);
		ret = -EINVAL;
		goto linker_path_write_out;
	}

	ret = len;

linker_path_write_out:
	kfree(path);

	return ret;
}

static const struct file_operations linker_path_file_ops = {
	.owner = THIS_MODULE,
	.write = linker_path_write,
};


/* ===========================================================================
 * =                           HANDLERS PATH                                 =
 * ===========================================================================
 */


static ssize_t handlers_path_write(struct file *file, const char __user *buf,
				   size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (path == NULL) {
		ret = -ENOMEM;
		goto handlers_path_write_out;
	}

	if (copy_from_user(path, buf, len)) {
		ret = -EINVAL;
		goto handlers_path_write_out;
	}

	path[len - 1] = '\0';

	if (preload_storage_set_handlers_info(path) != 0) {
		printk(PRELOAD_PREFIX "Cannot set handler path %s\n", path);
		ret = -EINVAL;
		goto handlers_path_write_out;
	}

	ret = len;

handlers_path_write_out:
	kfree(path);

	return ret;
}

static const struct file_operations handlers_path_file_ops = {
	.owner = THIS_MODULE,
	.write = handlers_path_write,
};




unsigned long preload_debugfs_r_debug_offset(void)
{
	return r_debug_offset;
}

int preload_debugfs_init(void)
{
	struct dentry *swap_dentry, *root, *loader, *open_p, *lib_path,
		  *bin_path, *bin_list, *bin_add, *bin_remove,
		  *linker_dir, *linker_path, *linker_offset, *handlers_path;
	int ret;

	ret = -ENODEV;
	if (!debugfs_initialized())
		goto fail;

	ret = -ENOENT;
	swap_dentry = swap_debugfs_getdir();
	if (!swap_dentry)
		goto fail;

	ret = -ENOMEM;
	root = debugfs_create_dir(PRELOAD_FOLDER, swap_dentry);
	if (IS_ERR_OR_NULL(root))
		goto fail;

	preload_root = root;

	loader = debugfs_create_dir(PRELOAD_LOADER, root);
	if (IS_ERR_OR_NULL(root)) {
		ret = -ENOMEM;
		goto remove;
	}

	open_p = debugfs_create_ptr(PRELOAD_LOADER_OFFSET, PRELOAD_DEFAULT_PERMS,
				    loader, &__loader_info.offset);
	if (IS_ERR_OR_NULL(open_p)) {
		ret = -ENOMEM;
		goto remove;
	}

	lib_path = debugfs_create_file(PRELOAD_LOADER_PATH, PRELOAD_DEFAULT_PERMS,
				       loader, NULL, &loader_path_file_ops);
	if (IS_ERR_OR_NULL(lib_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	bin_path = debugfs_create_dir(PRELOAD_BINARIES, root);
	if (IS_ERR_OR_NULL(bin_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	bin_list = debugfs_create_file(PRELOAD_BINARIES_LIST, PRELOAD_DEFAULT_PERMS,
				       bin_path, NULL, &bin_list_file_ops);
	if (IS_ERR_OR_NULL(bin_list)) {
		ret = -ENOMEM;
		goto remove;
	}

	bin_add = debugfs_create_file(PRELOAD_BINARIES_ADD, PRELOAD_DEFAULT_PERMS,
				       bin_path, NULL, &bin_add_file_ops);
	if (IS_ERR_OR_NULL(bin_add)) {
		ret = -ENOMEM;
		goto remove;
	}

	bin_remove = debugfs_create_file(PRELOAD_BINARIES_REMOVE,
					 PRELOAD_DEFAULT_PERMS, bin_path, NULL,
					 &bin_remove_file_ops);
	if (IS_ERR_OR_NULL(bin_remove)) {
		ret = -ENOMEM;
		goto remove;
	}

	linker_dir = debugfs_create_dir(PRELOAD_LINKER_DATA, root);
	if (IS_ERR_OR_NULL(linker_dir)) {
		ret = -ENOMEM;
		goto remove;
	}

	linker_path = debugfs_create_file(PRELOAD_LINKER_PATH,
					  PRELOAD_DEFAULT_PERMS, linker_dir, NULL,
					  &linker_path_file_ops);
	if (IS_ERR_OR_NULL(linker_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	linker_offset = debugfs_create_ptr(PRELOAD_LINKER_R_DEBUG_OFFSET,
					   PRELOAD_DEFAULT_PERMS, linker_dir,
					   &r_debug_offset);
	if (IS_ERR_OR_NULL(linker_offset)) {
		ret = -ENOMEM;
		goto remove;
	}

	handlers_path = debugfs_create_file(PRELOAD_HANDLERS_PATH,
					    PRELOAD_DEFAULT_PERMS, root, NULL,
					    &handlers_path_file_ops);
	if (IS_ERR_OR_NULL(handlers_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	return 0;

remove:

	debugfs_remove_recursive(root);

fail:
	printk(PRELOAD_PREFIX "Debugfs initialization failure: %d\n", ret);

	return ret;
}

void preload_debugfs_exit(void)
{
	if (preload_root)
		debugfs_remove_recursive(preload_root);
	preload_root = NULL;

	preload_module_set_not_ready();
	clean_loader_info();
}
