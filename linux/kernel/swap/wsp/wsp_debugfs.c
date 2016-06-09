/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <master/swap_debugfs.h>
#include "wsp.h"


static int do_write_cmd(const char *buf, size_t count)
{
	int n, ret = 0;
	char *name;
	unsigned long offset;

	name = kmalloc(count, GFP_KERNEL);
	if (name == NULL)
		return -ENOMEM;

	n = sscanf(buf, "%lx %s", &offset, name);
	if (n != 2) {
		ret = -EINVAL;
		goto free_name;
	}

	ret = wsp_set_addr(name, offset);

free_name:
	kfree(name);
	return ret;
}

/* ============================================================================
 * ===                         DEBUGFS FOR ENABLE                           ===
 * ============================================================================
 */
static ssize_t write_cmd(struct file *file, const char __user *user_buf,
			 size_t count, loff_t *ppos)
{
	enum { max_count = 1024 };
	int ret;
	char *buf;

	if (count > max_count)
		return -ENOMEM;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto free_buf;
	}

	buf[count] = '\0';
	ret = do_write_cmd(buf, count);

free_buf:
	kfree(buf);
	return ret ? ret : count;
}

static const struct file_operations fops_cmd = {
	.write =	write_cmd,
	.llseek =	default_llseek,
};




/* ============================================================================
 * ===                         DEBUGFS FOR ENABLE                           ===
 * ============================================================================
 */
static ssize_t read_enabled(struct file *file, char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	char buf[2];

	buf[0] = wsp_get_mode() == WSP_OFF ? '0' : '1';
	buf[1] = '\n';

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t write_enabled(struct file *file, const char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	int ret = 0;
	char buf[32];
	size_t buf_size;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = '\0';
	switch (buf[0]) {
	case '1':
		ret = wsp_set_mode(WSP_ON);
		break;
	case '0':
		ret = wsp_set_mode(WSP_OFF);
		break;
	default:
		return -EINVAL;
	}

	if (ret)
		return ret;

	return count;
}

static const struct file_operations fops_enabled = {
	.read =		read_enabled,
	.write =	write_enabled,
	.llseek =	default_llseek,
};


static struct dentry *wsp_dir;

void wsp_debugfs_exit(void)
{
	if (wsp_dir)
		debugfs_remove_recursive(wsp_dir);

	wsp_dir = NULL;
}

int wsp_debugfs_init(void)
{
	struct dentry *dentry;

	dentry = swap_debugfs_getdir();
	if (dentry == NULL)
		return -ENOENT;

	wsp_dir = debugfs_create_dir("wsp", dentry);
	if (wsp_dir == NULL)
		return -ENOMEM;

	dentry = debugfs_create_file("enabled", 0600, wsp_dir, NULL,
				     &fops_enabled);
	if (dentry == NULL)
		goto fail;

	dentry = debugfs_create_file("cmd", 0600, wsp_dir, NULL, &fops_cmd);
	if (dentry == NULL)
		goto fail;

	return 0;

fail:
	wsp_debugfs_exit();
	return -ENOMEM;
}
