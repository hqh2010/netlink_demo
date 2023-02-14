// SPDX-License-Identifier: GPL-2.0-only
/*
 *  add audit function based on extend attribute security.seaudit
 *
 *  This file contains the selinux hooks2 function implementations.
 *
 *  Authors:  zhoupeng, <zhoupeng@uniontech.com>
 *
 *  Copyright (C) 2022 UnionTech Technology, Inc.
 */
#ifdef CONFIG_SELINUX_SID2
#include <linux/fs.h>
#include <linux/namei.h>
#include <uapi/linux/if.h>
#include <linux/xattr.h>
#include <linux/rtnetlink.h>

#define DEV_CANT_WRITE_SELINUX_TYPE "deepin_ro_file_t"

int ifr_has_perm(const char * name, int flag) {
	struct file* devfile = 0;
	char acfileName[IFNAMSIZ+12];
	// control access device by sid2
	snprintf(acfileName, sizeof(acfileName), "/home/.sec/%s", name);
	devfile = filp_open(acfileName, flag, 0);
	printk("SELinux: tttt ifr_has_perm pid:%d comm:%s device name:%s PTR_ERR:%d IS_ERR:%d flag:%d", task_pid_nr(current), current->comm, name, PTR_ERR(devfile), IS_ERR(devfile), flag);
	if (-EACCES == PTR_ERR(devfile)) {//no permission to open the file
		return -EACCES;
	} else if (-EROFS == PTR_ERR(devfile)) {
		struct path ro_path;
		char acBuf[128];
		int err = kern_path(acfileName, O_RDONLY, &ro_path);
		if (err)
			return 0;
		err = vfs_getxattr(ro_path.dentry, XATTR_NAME_SELINUX2, acBuf,
				   sizeof(acBuf));
		path_put(&ro_path);
		if (strstr(acBuf, DEV_CANT_WRITE_SELINUX_TYPE) != 0)
			return -EACCES;
	} else if (!IS_ERR(devfile)) {//has been open successfully, need to close the file
		filp_close(devfile, NULL);
	}
	return 0;
}
EXPORT_SYMBOL(ifr_has_perm);

int if_netlink_has_perm(struct nlmsghdr *nlh)
{
	if (nlh->nlmsg_type < RTM_NEWLINK || nlh->nlmsg_type > RTM_GETROUTE) {
        return 0;
	}

	struct ifinfomsg *ifmsg = (struct ifinfomsg*)(NLMSG_DATA(nlh));
	struct net_device *dev;
	rcu_read_lock();
	dev = first_net_device(&init_net);
	while(dev) {
		if (ifmsg->ifi_index == dev->ifindex) {
			break;
		}
		dev = next_net_device(dev);
	}
	rcu_read_unlock();
	if (dev->name != NULL) {
		int r = ifr_has_perm(dev->name, O_RDWR);
		printk("SELinux: tttt if_netlink_has_perm device name:%s idx:%d r:%d", dev->name, ifmsg->ifi_index, r);
		return r;
	}
    return 0;
}
EXPORT_SYMBOL(if_netlink_has_perm);

int hci_has_perm(unsigned long num, int flag)
{
#define HCI_MAX_LEN 8
	struct file* devfile;
	char acfileName[HCI_MAX_LEN + 14];
	// control access device by sid2
	snprintf(acfileName, sizeof(acfileName), "/home/.sec/hci%ld", num);
	devfile = filp_open(acfileName, flag, 0);
	if (-EACCES == PTR_ERR(devfile)) {
		return -EACCES;
	} else if (!IS_ERR(devfile)) {
		filp_close(devfile, NULL);//need to close file
	}

	return 0;
}
EXPORT_SYMBOL(hci_has_perm);

int open_dev_has_perm(const char* dev_name, unsigned long mount_flags)
{
#define DEV_NAME_MIN_LEN 6
	int perm = 0, len = 0;;
	struct file* devfile;
	char acFileName[128];

	if (NULL==dev_name)
	 	return 0;
	len = strlen(dev_name);
	if (len < DEV_NAME_MIN_LEN)
		return 0;
	if (strncmp(dev_name, "/dev/", DEV_NAME_MIN_LEN -1 ) != 0) {
		return 0;
	}

	len = snprintf(acFileName, sizeof(acFileName), "/home/.sec/%s", &dev_name[DEV_NAME_MIN_LEN-1]);
	if (len >= sizeof(acFileName)) {
		goto out_perm;
	}
	if (mount_flags & MS_RDONLY) {
		devfile = filp_open(acFileName, O_RDONLY, 0);
	} else {
		devfile = filp_open(acFileName, O_RDWR, 0);
	}
	if (-EACCES == PTR_ERR(devfile)) {//no permission to open the file
		perm = -EACCES;
		goto out_perm;
	} else if (!IS_ERR(devfile)) {//has been open successfully, need to close the file
		filp_close(devfile, NULL);
	}
	// if /home/.sec/name doesn't existed, perm ==0
out_perm:
	return perm;
}
EXPORT_SYMBOL(open_dev_has_perm);

int cdrom_has_perm(const char * name, int flag)
{
	struct file* devfile = 0;
	int len;
	// name size should be less than 32B, the default name is sr0, sr1...
#define CDROM_NAME_SIZE 32
	char acfileName[CDROM_NAME_SIZE + 12];
	if (0 == name)  {
		printk("cdrom name is empty, no permission control\n");
		return 0;
	}
	// control access device by sid2
	len = snprintf(acfileName, sizeof(acfileName), "/home/.sec/%s", name);
	if (len >= (CDROM_NAME_SIZE+12)) {
		printk("too long name for cdrom, dont check permission for %s\n", name);
		return 0;
	}
	devfile = filp_open(acfileName, flag, 0);
	if (-EACCES == PTR_ERR(devfile)) { // no permission to open the file
		return -EACCES;
	}
	else if (!IS_ERR(devfile)) { // has been open successfully, need to close the file
		filp_close(devfile, NULL);
	}
	return 0;
}
EXPORT_SYMBOL(cdrom_has_perm);
#endif
