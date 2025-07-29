/*************************************************************************
    > File Name: kDemo_2.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com 
    > Created Time: Sat Oct 14 16:17:20 2023
 ************************************************************************/

#include <linux/init.h>         /* __init   __exit */
#include <linux/module.h>       /* module_init  module_exit */

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";
extern char *demo1_export_test;

static int __init demo_2_init(void)
{    
    printk(KERN_INFO "module %s init desc:%s\n", __func__, init_desc);
    printk(KERN_INFO "synbol from demo1 %s\n", demo1_export_test);

    return 0;
}

static void __exit demo_2_exit(void)
{
    printk(KERN_INFO "module %s exit desc:%s\n", __func__, exit_desc);

    return;
}

module_init(demo_2_init);
module_exit(demo_2_exit);

module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

MODULE_LICENSE("GPL v2");                       /* 描述模块的许可证 */
MODULE_AUTHOR("Lhj <872648180@qq.com>");        /* 描述模块的作者 */
MODULE_DESCRIPTION("base demo for learning");   /* 描述模块的介绍信息 */
MODULE_ALIAS("base demo");                      /* 描述模块的别名信息 */
