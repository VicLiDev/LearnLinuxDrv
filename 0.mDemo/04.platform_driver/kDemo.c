/*************************************************************************
    > File Name: kDemo.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com 
    > Created Time: Fri Oct 13 16:02:51 2023
 ************************************************************************/

#include <linux/init.h>         /* __init   __exit */
#include <linux/module.h>       /* module_init  module_exit */
#include <linux/cdev.h>
#include <linux/kernel.h>
/* file opt */
#include <linux/uaccess.h>
#include <linux/fs.h>

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of_device.h>


static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";
module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

static int m_miscdev_open(struct inode *inode, struct file *file);
static int m_miscdev_release(struct inode *inode, struct file *file);
static long m_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t m_miscdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t m_miscdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

/* initialize file_operations */
static const struct file_operations m_miscdev_fops = {
    .owner      = THIS_MODULE,
    .open       = m_miscdev_open,
    .release    = m_miscdev_release,
    .unlocked_ioctl = m_miscdev_ioctl,
    .read       = m_miscdev_read,
    .write       = m_miscdev_write
};

struct m_plt_misc_dev {
	char data[100];
	struct miscdevice miscdev;
};

static int m_miscdev_open(struct inode *inode, struct file *file)
{
	printk("func:%s line:%d\n", __func__, __LINE__);

    return 0;
}
static int m_miscdev_release(struct inode *inode, struct file *file)
{
	printk("func:%s line:%d\n", __func__, __LINE__);

    return 0;
}

static long m_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	printk("func:%s line:%d\n", __func__, __LINE__);

    return 0;
}

static ssize_t m_miscdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	printk("func:%s line:%d\n", __func__, __LINE__);

    return count;
}

static ssize_t m_miscdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	printk("func:%s line:%d\n", __func__, __LINE__);

    return count;
}

static int m_plt_probe(struct platform_device *pdev)
{
    struct m_plt_misc_dev *m_misc_dev = NULL;
	int ret;

	dev_info(&pdev->dev, "func:%s line:%d\n", __func__, __LINE__);

	m_misc_dev = devm_kzalloc(&pdev->dev, sizeof(*m_misc_dev), GFP_KERNEL);
	if (!m_misc_dev)
		return -ENOMEM;
	platform_set_drvdata(pdev, m_misc_dev);

	m_misc_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	m_misc_dev->miscdev.name = "m_misc_demo";
	m_misc_dev->miscdev.fops = &m_miscdev_fops;
	ret = misc_register(&m_misc_dev->miscdev);
	if (ret < 0) {
	    dev_info(&pdev->dev, "func:%s line:%d misc register failed! ret:%d\n",
                __func__, __LINE__, ret);
		return ret;
    } else {
	    dev_info(&pdev->dev, "func:%s line:%d misc register sucess!\n",
                __func__, __LINE__);
    }

    return ret;
}

static int m_plt_remove(struct platform_device *pdev)
{
	struct m_plt_misc_dev *m_misc_dev = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "func:%s line:%d\n", __func__, __LINE__);

	misc_deregister(&m_misc_dev->miscdev);

	return 0;
}

static struct platform_driver m_plt_driver = {
	.driver = {
		.name = "m_plt_demo",
		.owner = THIS_MODULE,
	},
	.probe = m_plt_probe,
	.remove = m_plt_remove,
};

module_platform_driver(m_plt_driver);


MODULE_LICENSE("GPL v2");                       /* 描述模块的许可证 */
MODULE_AUTHOR("Lhj <872648180@qq.com>");        /* 描述模块的作者 */
MODULE_DESCRIPTION("base demo for learning");   /* 描述模块的介绍信息 */
MODULE_ALIAS("plt drv");                        /* 描述模块的别名信息 */
MODULE_VERSION(DEMO_GIT_VERSION);
