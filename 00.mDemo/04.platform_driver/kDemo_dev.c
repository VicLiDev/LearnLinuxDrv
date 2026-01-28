/*************************************************************************
    > File Name: kDemo_dev.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com 
    > Created Time: Mon Oct 23 09:07:12 2023
 ************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>

static struct platform_device *m_plt_dev;

static int __init m_pltdev_init(void)
{
	int ret;

	m_plt_dev = platform_device_alloc("m_plt_demo", -1);
	if (!m_plt_dev)
		return -ENOMEM;

	ret = platform_device_add(m_plt_dev);
	if (ret) {
		platform_device_put(m_plt_dev);
		return ret;
	} else
	    printk("func:%s line:%d plt dev register sucess!\n",
                __func__, __LINE__);

	return 0;

}
module_init(m_pltdev_init);

static void __exit m_pltdev_exit(void)
{
	platform_device_unregister(m_plt_dev);
	printk("func:%s line:%d plt dev unregister sucess!\n",
            __func__, __LINE__);
}
module_exit(m_pltdev_exit);

MODULE_LICENSE("GPL v2");                       /* 描述模块的许可证 */
MODULE_AUTHOR("Lhj <872648180@qq.com>");        /* 描述模块的作者 */
MODULE_DESCRIPTION("base demo for learning");   /* 描述模块的介绍信息 */
MODULE_ALIAS("plt dev");                        /* 描述模块的别名信息 */
MODULE_VERSION(DEMO_GIT_VERSION);
