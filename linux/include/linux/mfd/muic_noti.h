/*
 * include/linux/mfd/muic_noti.h
 *
 * SEC MUIC Notification header file
 *
 */

#ifndef LINUX_MFD_MUICNOTI_H
#define LINUX_MFD_MUICNOTI_H

extern int unregister_muic_notifier(struct notifier_block *nb);
extern int register_muic_notifier(struct notifier_block *nb);

#define MUIC_OTG_DETACH_NOTI		0x0004
#define MUIC_OTG_ATTACH_NOTI		0x0003
#define MUIC_VBUS_NOTI				0x0002
#define MUIC_USB_ATTACH_NOTI		0x0001
#define MUIC_USB_DETACH_NOTI		0x0000

struct muic_notifier_param {
	uint32_t vbus_status;
	int cable_type;
};

#endif /* LINUX_MFD_SM5504_H */
