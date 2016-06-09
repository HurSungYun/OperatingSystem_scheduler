/* linux/drivers/video/mdnie.c
 *
 * Register interface file for Samsung mDNIe driver
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/magic.h>

#include "mdnie_lite.h"
#include "../sprdfb.h"
#include "../sprdfb_panel.h"

#include "mdnie_lite_table_z3.h"

#define MDNIE_SYSFS_PREFIX		"/opt/usr/media/"

#define IS_SCENARIO(idx)		(idx < SCENARIO_MAX)
#define IS_ACCESSIBILITY(idx)		(idx && (idx < ACCESSIBILITY_MAX))
#define IS_OUTDOOR(idx)			(idx == OUTDOOR_ON)

#define SCENARIO_IS_VALID(idx)   IS_SCENARIO(idx)

/* Split 16 bit as 8bit x 2 */
#define GET_MSB_8BIT(x)		((x >> 8) & (BIT(8) - 1))
#define GET_LSB_8BIT(x)		((x >> 0) & (BIT(8) - 1))

static struct class *mdnie_class;

static int parse_text(char *src)
{
	char *str = NULL;
	unsigned int val1, val2, val3 = 0;
	int i = 0;
	int j = 0;
	int len = 0;
	int reg_num = 0;
	int size[ARRAY_SIZE(mdnie_tune_on_1)] = {0,};
	int ret;

	while ((str = strsep((char **)&src, "\n"))) {
		ret = sscanf(str, "0x%2x,", &reg_num);
		if (ret == 1)
			break;
	}
	while ((str = strsep((char **)&src, "\n"))) {
		ret = sscanf(str, "0x%2x,0x%2x,", &val1, &val2);
		j = 2;
		if (ret == 2) {
			mdnie_tune_on_1[i].data[j] = val1;
			size[i] = val2 + 3;
			j++;
			for (len = 0; len < val2; len++) {
				str = strsep((char **)&src, "\n");
				if (str) {
					ret = sscanf(str, "0x%2x,", &val3);
					if (ret == 1) {
						mdnie_tune_on_1[i].data[j]  = val3;
						j++;
					}
				}
			}
			reg_num--;
			if (reg_num == 0)
				break;
			i++;
		}

	}

	for (i = 0; i < ARRAY_SIZE(mdnie_tune_on_1); i++) {
		pr_debug("[MDNIE][%d]", i);
		for (j = 0; j < size[i]; j++) {
			if ((j % 5) == 0)
				pr_debug("\n[MDNIE]");
			pr_debug("0x%2x ", mdnie_tune_on_1[i].data[j]);
		}
		pr_debug("\n");
	}

	return 0;
}

static int mdnie_request_table(char *path, struct mdnie_table *s)
{
	char *dp;
	long l;
	int ret;
	loff_t pos;
	struct file *filp;
	mm_segment_t fs;

	pr_info("%s: loading file : [%s]\n", __func__, path);

	fs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("%s:File open failed\n", __func__);
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		pr_err("%s:mem alloc fail for tuning file\n", __func__);
		filp_close(filp, current->files);
		return -1;
	}

	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	if (ret != l) {
		pr_err("%s: vfs_read() filed ret : %d\n", __func__, ret);
		kfree(dp);
		filp_close(filp, current->files);
		return -1;
	}

	filp_close(filp, current->files);

	set_fs(fs);
	parse_text(dp);

	return 0;
}

/* Do not call mdnie write directly */
static int mdnie_write(struct sprdfb_device *fb_dev, struct mdnie_table *table)
{
	int ret = 0;
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	if (mdnie->enable)
		ret = mdnie->ops.write(fb_dev, table->tune, MDNIE_CMD_MAX);

	return ret;
}

static int mdnie_write_table(struct sprdfb_device *fb_dev, struct mdnie_table *table)
{
	int i, ret = 0;
	struct mdnie_table *buf = NULL;
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	for (i = 0; i < MDNIE_CMD_MAX; i++) {
		if (IS_ERR_OR_NULL(table->tune[i].sequence)) {
			dev_err(mdnie->dev, "mdnie sequence %s is null, %lx\n",
					table->name, (unsigned long)table->tune[i].sequence);
			return -EPERM;
		}
	}

	mutex_lock(&mdnie->dev_lock);

	buf = table;

	ret = mdnie_write(fb_dev, buf);

	mutex_unlock(&mdnie->dev_lock);

	return ret;
}

static struct mdnie_table *mdnie_find_table(struct mdnie_lite_device *mdnie)
{
	struct mdnie_table *table = NULL;

	mutex_lock(&mdnie->lock);

	if (IS_ACCESSIBILITY(mdnie->accessibility)) {
		table = &accessibility_table[mdnie->accessibility];
		goto exit;
	} else if (IS_OUTDOOR(mdnie->outdoor)) {
		table = &outdoor_table[mdnie->outdoor];
		goto exit;
	} else if (IS_SCENARIO(mdnie->scenario)) {
		table = &tuning_table[mdnie->scenario][mdnie->mode];
		goto exit;
	}

exit:
	mutex_unlock(&mdnie->lock);
	pr_debug("[mdnie]: Table got is [%s]\n", table->name);

	return table;
}

static void mdnie_update_sequence(struct sprdfb_device *fb_dev, struct mdnie_table *table)
{
	struct mdnie_table *t;
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	if (mdnie->tuning) {
		struct mdnie_table tbl[] = {MDNIE_SET(mdnie_tune_on)};
		mdnie_request_table(mdnie->path, table);
		t = &(tbl[0]);

		if (!IS_ERR_OR_NULL(t) && !IS_ERR_OR_NULL((t)->name))
			mdnie_write_table(fb_dev, t);
		else
			mdnie_write_table(fb_dev, table);
	} else
		mdnie_write_table(fb_dev, table);

	return;
}

static void mdnie_update(struct sprdfb_device *fb_dev, struct mdnie_lite_device *mdnie)
{
	struct mdnie_table *table = NULL;

	if (!mdnie->enable) {
		dev_err(mdnie->dev, "mdnie state is off\n");
		return;
	}

	table = mdnie_find_table(mdnie);
	if (!IS_ERR_OR_NULL(table) && !IS_ERR_OR_NULL(table->name)) {
		mdnie_update_sequence(fb_dev, table);
		dev_info(mdnie->dev, "%s\n", table->name);
	}

	return;
}

void mdnie_state_restore(struct sprdfb_device *fb_dev)
{
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;
	mdnie->enable = 1;
	mdnie_update(fb_dev, mdnie);

	return;
}

static ssize_t mdnie_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	return sprintf(buf, "%d\n", mdnie->mode);
}

static int mdnie_calibration(int *r)
{
	int ret = 0;

	if (r[1] > 0) {
		if (r[3] > 0)
			ret = 3;
		else
			ret = (r[4] < 0) ? 1 : 2;
	} else {
		if (r[2] < 0) {
			if (r[3] > 0)
				ret = 9;
			else
				ret = (r[4] < 0) ? 7 : 8;
			} else {
				if (r[3] > 0)
					ret = 6;
				else
					ret = (r[4] < 0) ? 4 : 5;
			}
	}

	pr_info("%d, %d, %d, %d, tune%d\n", r[1], r[2], r[3], r[4], ret);

	return ret;
}

static void update_color_position(struct mdnie_lite_device *mdnie, unsigned int idx)
{
	u8 mode, scenario;
	mdnie_t *wbuf;

	mutex_lock(&mdnie->lock);

	for (mode = 0; mode < MODE_MAX; mode++) {
		if (mode == MODE_AUTO || mode == MODE_DYNAMIC) {
			for (scenario = 0; scenario < SCENARIO_MAX; scenario++) {
				wbuf = tuning_table[scenario][mode].tune[MDNIE_CMD1].sequence->data;
				if (IS_ERR_OR_NULL(wbuf))
					continue;
				if (scenario != SCENARIO_EBOOK && scenario != SCEANRIO_GRAY &&
					scenario != SCENARIO_NEGATIVE) {
					wbuf[MDNIE_WHITE_R] = coordinate_data_1[idx][0];
					wbuf[MDNIE_WHITE_G] = coordinate_data_1[idx][1];
					wbuf[MDNIE_WHITE_B] = coordinate_data_1[idx][2];
				}
			}
		}
	}

	mutex_unlock(&mdnie->lock);
}

static int get_panel_coordinate(struct mdnie_lite_device *mdnie, int *result)
{
	int ret = 0;

	unsigned short x, y;

	x = mdnie->co_ordinates[0];
	y = mdnie->co_ordinates[1];


	result[1] = COLOR_OFFSET_F1(x, y);
	result[2] = COLOR_OFFSET_F2(x, y);
	result[3] = COLOR_OFFSET_F3(x, y);
	result[4] = COLOR_OFFSET_F4(x, y);

	ret = mdnie_calibration(result);
	dev_info(mdnie->dev, "[mdnie] %s: x = %d, y = %d, idx = %d\n", __func__, x, y, ret);

	mdnie->color_correction = 1;

	return ret;
}

static ssize_t mdnie_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	unsigned int value = 0;
	int ret;
	int result[5] = {0,};

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: value=%d\n", __func__, value);

	if (value >= MODE_MAX) {
		value = MODE_STANDARD;
		return -EINVAL;
	}

	mutex_lock(&mdnie->lock);
	mdnie->mode = value;
	mutex_unlock(&mdnie->lock);

	if (!mdnie->color_correction) {
		ret = get_panel_coordinate(mdnie, result);
		if (ret > 0)
			update_color_position(mdnie, ret);
	}

	mdnie_update(fb_dev, mdnie);

	return count;
}


static ssize_t mdnie_scenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	return sprintf(buf, "%d\n", mdnie->scenario);
}

static ssize_t mdnie_scenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: value=%d\n", __func__, value);

	if (!SCENARIO_IS_VALID(value))
		value = SCENARIO_UI;

	mutex_lock(&mdnie->lock);
	mdnie->scenario = value;
	mutex_unlock(&mdnie->lock);

	mdnie_update(fb_dev, mdnie);

	return count;
}

static ssize_t mdnie_tune_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->tuning);
}

static ssize_t mdnie_tune_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	if (sysfs_streq(buf, "0") || sysfs_streq(buf, "1")) {
		ret = kstrtouint(buf, 0, &mdnie->tuning);
		if (ret < 0)
			return ret;
		if (!mdnie->tuning)
			memset(mdnie->path, 0, sizeof(mdnie->path));

		dev_info(dev, "%s: %s\n", __func__, mdnie->tuning ? "enable" : "disable");
	} else {
		if (!mdnie->tuning)
			return count;

		if (count > (sizeof(mdnie->path) - sizeof(MDNIE_SYSFS_PREFIX))) {
			dev_err(dev, "file name %s is too long\n", mdnie->path);
			return -ENOMEM;
		}

		memset(mdnie->path, 0, sizeof(mdnie->path));
		snprintf(mdnie->path, sizeof(MDNIE_SYSFS_PREFIX) + count-1, "%s%s", MDNIE_SYSFS_PREFIX, buf);
		dev_info(dev, "%s: %s\n", __func__, mdnie->path);

		mdnie_update(fb_dev, mdnie);
	}

	return count;
}

static ssize_t mdnie_accessibility_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	return sprintf(buf, "%d\n", mdnie->accessibility);
}

static ssize_t mdnie_accessibility_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;
	unsigned int value, s[9], i = 0;
	int ret;
	mdnie_t *wbuf;

	ret = sscanf(buf, "%d %x %x %x %x %x %x %x %x %x",
			&value, &s[0], &s[1], &s[2], &s[3],
			&s[4], &s[5], &s[6], &s[7], &s[8]);

	dev_info(dev, "%s: value=%d\n", __func__, value);

	if (ret < 0)
		return ret;
	else {
		if (value >= ACCESSIBILITY_MAX)
			value = ACCESSIBILITY_OFF;

		mutex_lock(&mdnie->lock);
		mdnie->accessibility = value;
		mutex_unlock(&mdnie->lock);

		mdnie_update(fb_dev, mdnie);
	}

	return count;
}

static ssize_t mdnie_outdoor_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->outdoor);
}

static ssize_t mdnie_outdoor_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct sprdfb_device *fb_dev = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = fb_dev->panel->mdnie;
	int value;
	int ret;

	ret = sscanf(buf, "%d", &value);
	if (ret < 0)
		return ret;
	else {
		if (value >= OUTDOOR_MAX)
			value = OUTDOOR_OFF;

		mutex_lock(&mdnie->lock);
		mdnie->outdoor = value;
		mutex_unlock(&mdnie->lock);
	}
	mdnie_update(fb_dev, mdnie);

	return size;
}

static struct device_attribute mdnie_attributes[] = {
	__ATTR(mode, 0664, mdnie_mode_show, mdnie_mode_store),
	__ATTR(scenario, 0664, mdnie_scenario_show, mdnie_scenario_store),
	__ATTR(tune, 0664, mdnie_tune_show, mdnie_tune_store),
	__ATTR(accessibility, 0664, mdnie_accessibility_show, mdnie_accessibility_store),
	__ATTR(outdoor, 0664, mdnie_outdoor_show, mdnie_outdoor_store),
	__ATTR_NULL,
};

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct mdnie_lite_device *mdnie;
	struct fb_event *evdata = data;
	int fb_blank;

	mdnie = container_of(self, struct mdnie_lite_device, fb_notif);

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node != 0)
		return 0;

	if (fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(&mdnie->lock);
		mdnie->enable = 1;
		mutex_unlock(&mdnie->lock);
		/* mdnie_update(fb_dev, mdnie); */

	} else if (fb_blank == FB_BLANK_POWERDOWN) {
		mutex_lock(&mdnie->lock);
		mdnie->enable = 0;
		mutex_unlock(&mdnie->lock);
	}

	return 0;
}

static int mdnie_register_fb(struct mdnie_lite_device *mdnie)
{
	memset(&mdnie->fb_notif, 0, sizeof(mdnie->fb_notif));
	mdnie->fb_notif.notifier_call = fb_notifier_callback;
	return fb_register_client(&mdnie->fb_notif);
}

int sprdfb_mdnie_reg(struct sprdfb_device *dev, mdnie_w w, mdnie_r r, mdnie_c c)
{
	int ret = 0;
	struct panel_spec *panel = dev->panel;
	struct mdnie_lite_device *mdnie;

	mdnie_class = class_create(THIS_MODULE, "extension");
	if (IS_ERR_OR_NULL(mdnie_class)) {
		pr_err("failed to create mdnie class\n");
		ret = -EINVAL;
		goto error0;
	}

	mdnie_class->dev_attrs = mdnie_attributes;

	mdnie = kzalloc(sizeof(struct mdnie_lite_device), GFP_KERNEL);
	if (!mdnie) {
		pr_err("failed to allocate mdnie\n");
		ret = -ENOMEM;
		goto error1;
	}

	mdnie->dev = device_create(mdnie_class, NULL, 0, NULL, "mdnie");
	if (IS_ERR_OR_NULL(mdnie->dev)) {
		pr_err("failed to create mdnie device\n");
		ret = -EINVAL;
		goto error2;
	}

	mdnie->scenario = SCENARIO_UI;
	mdnie->mode = MODE_STANDARD;
	mdnie->accessibility = ACCESSIBILITY_OFF;
	mdnie->outdoor = OUTDOOR_OFF;

	mdnie->ops.write = w;
	mdnie->ops.read = r;
	mdnie->ops.color = c;
	mdnie->co_ordinates[0] = 0;
	mdnie->co_ordinates[1] = 0;
	mdnie->color_correction = 0;

	mdnie->ops.color(dev, mdnie->co_ordinates);

	mutex_init(&mdnie->lock);
	mutex_init(&mdnie->dev_lock);

	panel->mdnie = mdnie;
	dev_set_drvdata(mdnie->dev, dev);

	mdnie_register_fb(mdnie);

	mdnie->enable = 1;
	mdnie_update(dev, mdnie);

	dev_info(mdnie->dev, "registered successfully\n");

	return 0;

error2:
	kfree(mdnie);
error1:
	class_destroy(mdnie_class);
error0:
	return ret;
}
