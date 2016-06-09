/**
 * parser/us_inst.c
 * @author Vyacheslav Cherkashin
 *
 * @section LICENSE
 *
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
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * User-space instrumentation controls.
 */


#include <linux/slab.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <us_manager/pf/pf_group.h>
#include <us_manager/probes/probes.h>

#include "msg_parser.h"
#include "us_inst.h"
#include "usm_msg.h"


struct pfg_item {
	struct list_head list;
	struct pf_group *pfg;
};


static LIST_HEAD(pfg_item_list);
static DEFINE_SPINLOCK(pfg_item_lock);


static struct pfg_msg_cb msg_cb = {
	.msg_info = usm_msg_info,
	.msg_status_info = usm_msg_status_info,
	.msg_term = usm_msg_term,
	.msg_map = usm_msg_map,
	.msg_unmap = usm_msg_unmap
};

static struct pfg_item *pfg_item_create(struct pf_group *pfg)
{
	int ret;
	struct pfg_item *item;

	ret = pfg_msg_cb_set(pfg, &msg_cb);
	if (ret)
		return ERR_PTR(ret);

	item = kmalloc(sizeof(*item), GFP_KERNEL);
	if (item == NULL)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&item->list);
	item->pfg = pfg;

	return item;
}

static void pfg_item_free(struct pfg_item *item)
{
	pfg_msg_cb_reset(item->pfg);
	kfree(item);
}

/* called with pfg_item_lock held */
static bool pfg_check(struct pf_group *pfg)
{
	struct pfg_item *item;

	list_for_each_entry(item, &pfg_item_list, list) {
		if (item->pfg == pfg)
			return true;
	}

	return false;
}

static int pfg_add(struct pf_group *pfg)
{
	bool already;

	spin_lock(&pfg_item_lock);
	already = pfg_check(pfg);
	spin_unlock(&pfg_item_lock);

	if (already) {
		put_pf_group(pfg);
	} else {
		struct pfg_item *item;

		item = pfg_item_create(pfg);
		if (IS_ERR(item))
			return PTR_ERR(item);

		spin_lock(&pfg_item_lock);
		list_add(&item->list, &pfg_item_list);
		spin_unlock(&pfg_item_lock);
	}

	return 0;
}

void pfg_put_all(void)
{
	LIST_HEAD(tmp_list);
	struct pfg_item *item, *n;

	spin_lock(&pfg_item_lock);
	list_splice_init(&pfg_item_list, &tmp_list);
	spin_unlock(&pfg_item_lock);

	list_for_each_entry_safe(item, n, &tmp_list, list) {
		struct pf_group *pfg = item->pfg;

		list_del(&item->list);
		pfg_item_free(item);
		put_pf_group(pfg);
	}
}

static int mod_func_inst(struct func_inst_data *func, struct pf_group *pfg,
			 struct dentry *dentry, enum MOD_TYPE mt)
{
	int ret;

	switch (mt) {
	case MT_ADD:
		ret = pf_register_probe(pfg, dentry, func->addr,
					&func->probe_i);
		break;
	case MT_DEL:
		ret = pf_unregister_probe(pfg, dentry, func->addr);
		break;
	default:
		printk(KERN_INFO "ERROR: mod_type=0x%x\n", mt);
		ret = -EINVAL;
	}

	return ret;
}

static int mod_lib_inst(struct lib_inst_data *lib, struct pf_group *pfg,
			enum MOD_TYPE mt)
{
	int ret = 0, i;
	struct dentry *dentry;

	dentry = dentry_by_path(lib->path);
	if (dentry == NULL) {
		printk(KERN_INFO "Cannot get dentry by path %s\n", lib->path);
		return -EINVAL;
	}

	for (i = 0; i < lib->cnt_func; ++i) {
		ret = mod_func_inst(lib->func[i], pfg, dentry, mt);
		if (ret) {
			printk(KERN_INFO "Cannot mod func inst, ret = %d\n",
			       ret);
			return ret;
		}
	}

	return ret;
}

static int get_pfg_by_app_info(struct app_info_data *app_info,
			       struct pf_group **pfg)
{
	struct dentry *dentry;

	dentry = dentry_by_path(app_info->exec_path);
	if (dentry == NULL)
		return -EINVAL;

	switch (app_info->app_type) {
	case AT_PID:
		if (app_info->tgid == 0) {
			if (app_info->exec_path[0] == '\0')
				*pfg = get_pf_group_dumb(dentry);
			else
				goto pf_dentry;
		} else
			*pfg = get_pf_group_by_tgid(app_info->tgid, dentry);
		break;
	case AT_TIZEN_WEB_APP:
		*pfg = get_pf_group_by_comm(app_info->app_id, dentry);
		break;
	case AT_TIZEN_NATIVE_APP:
	case AT_COMMON_EXEC:
 pf_dentry:
		*pfg = get_pf_group_by_dentry(dentry, dentry);
		break;
	default:
		printk(KERN_INFO "ERROR: app_type=0x%x\n", app_info->app_type);
		return -EINVAL;
	}

	return 0;
}

static int mod_us_app_inst(struct app_inst_data *app_inst, enum MOD_TYPE mt)
{
	int ret, i;
	struct pf_group *pfg;
	struct dentry *dentry;

	ret = get_pfg_by_app_info(app_inst->app_info, &pfg);
	if (ret) {
		printk(KERN_INFO "Cannot get pfg by app info, ret = %d\n", ret);
		return ret;
	}

	ret = pfg_add(pfg);
	if (ret) {
		put_pf_group(pfg);
		printk(KERN_INFO "Cannot pfg_add, ret=%d\n", ret);
		return ret;
	}

	for (i = 0; i < app_inst->cnt_func; ++i) {
		/* TODO: */
		dentry = dentry_by_path(app_inst->app_info->exec_path);
		if (dentry == NULL) {
			printk(KERN_INFO "Cannot find dentry by path %s\n",
			       app_inst->app_info->exec_path);
			return -EINVAL;
		}

		ret = mod_func_inst(app_inst->func[i], pfg, dentry, mt);
		if (ret) {
			printk(KERN_INFO "Cannot mod func inst, ret = %d\n",
			       ret);
			return ret;
		}
	}

	for (i = 0; i < app_inst->cnt_lib; ++i) {
		ret = mod_lib_inst(app_inst->lib[i], pfg, mt);
		if (ret) {
			printk(KERN_INFO "Cannot mod lib inst, ret = %d\n",
			       ret);
			return ret;
		}
	}

	return 0;
}

/**
 * @brief Registers probes.
 *
 * @param us_inst Pointer to the target us_inst_data struct.
 * @param mt Modificator, indicates whether we install or remove probes.
 * @return 0 on suceess, error code on error.
 */
int mod_us_inst(struct us_inst_data *us_inst, enum MOD_TYPE mt)
{
	u32 i;
	int ret;

	for (i = 0; i < us_inst->cnt; ++i) {
		ret = mod_us_app_inst(us_inst->app_inst[i], mt);
		if (ret) {
			printk(KERN_INFO "Cannot mod us app inst, ret = %d\n",
			       ret);
			return ret;
		}
	}

	return 0;
}
