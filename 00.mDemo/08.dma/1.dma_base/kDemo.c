/*************************************************************************
    > File Name: kDemo.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com
    > Created Time: Fri Oct 13 16:02:51 2023
 ************************************************************************/


#include <linux/init.h>         /* __init   __exit */
#include <linux/module.h>       /* module_init  module_exit */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
/* file opt */
#include <linux/uaccess.h>
#include <linux/fs.h>

/* DMA related headers */
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>


#define MAX_DEV 2
#define CLS_NAME "m_class_name"

/* DMA buffer sizes */
#define COHERENT_BUF_SIZE     (PAGE_SIZE * 4)    /* 16KB coherent DMA buffer */
#define SINGLE_BUF_SIZE       (PAGE_SIZE * 2)    /* 8KB streaming DMA buffer */
#define DMA_POOL_SIZE         256                /* 256 bytes per pool allocation */
#define DMA_POOL_BOUNDARY     0                  /* No boundary restriction */
#define SG_NENTS              4                  /* Number of scatter-gather entries */
#define SG_PAGE_SIZE          PAGE_SIZE          /* Each sg entry is one page */

/* IOCTL commands for DMA operations */
#define DMA_MAGIC             'D'

/* Coherent DMA operations */
#define DMA_IOCTL_ALLOC_COHERENT    _IOWR(DMA_MAGIC, 1, struct dma_ioctl_param)
#define DMA_IOCTL_FREE_COHERENT     _IOW(DMA_MAGIC, 2, struct dma_ioctl_param)
#define DMA_IOCTL_READ_COHERENT     _IOWR(DMA_MAGIC, 3, struct dma_ioctl_param)
#define DMA_IOCTL_WRITE_COHERENT    _IOW(DMA_MAGIC, 4, struct dma_ioctl_param)

/* Streaming DMA single mapping operations */
#define DMA_IOCTL_MAP_SINGLE        _IOWR(DMA_MAGIC, 5, struct dma_ioctl_param)
#define DMA_IOCTL_UNMAP_SINGLE      _IOW(DMA_MAGIC, 6, struct dma_ioctl_param)
#define DMA_IOCTL_SYNC_SINGLE       _IOW(DMA_MAGIC, 7, struct dma_ioctl_param)

/* Scatter-gather DMA operations */
#define DMA_IOCTL_MAP_SG            _IOWR(DMA_MAGIC, 8, struct dma_ioctl_param)
#define DMA_IOCTL_UNMAP_SG          _IOW(DMA_MAGIC, 9, struct dma_ioctl_param)
#define DMA_IOCTL_SYNC_SG           _IOW(DMA_MAGIC, 10, struct dma_ioctl_param)

/* DMA pool operations */
#define DMA_IOCTL_POOL_CREATE       _IOWR(DMA_MAGIC, 11, struct dma_ioctl_param)
#define DMA_IOCTL_POOL_ALLOC        _IOWR(DMA_MAGIC, 12, struct dma_ioctl_param)
#define DMA_IOCTL_POOL_FREE         _IOW(DMA_MAGIC, 13, struct dma_ioctl_param)
#define DMA_IOCTL_POOL_DESTROY      _IOW(DMA_MAGIC, 14, struct dma_ioctl_param)

/* DMA information query */
#define DMA_IOCTL_GET_INFO          _IOWR(DMA_MAGIC, 15, struct dma_ioctl_param)

/* DMA mask configuration */
#define DMA_IOCTL_SET_MASK          _IOW(DMA_MAGIC, 16, struct dma_ioctl_param)

/* IOCTL parameter structure */
struct dma_ioctl_param {
    unsigned long size;         /* Buffer size */
    unsigned long dma_addr;     /* DMA address (returned from kernel) */
    unsigned long user_addr;    /* User space buffer address */
    int direction;              /* DMA direction */
    int count;                  /* Count for scatter-gather */
    unsigned int mask_bits;     /* DMA mask bits (32 or 64) */
    int result;                 /* Result code */
    char data[64];              /* Data buffer for small transfers */
};

/* DMA directions for userspace */
enum dma_user_dir {
    DMA_USER_TO_DEVICE = 1,
    DMA_USER_FROM_DEVICE = 2,
    DMA_USER_BIDIRECTIONAL = 3
};

static char *init_desc = "default init desc";
static char *exit_desc = "default exit desc";

module_param(init_desc, charp, S_IRUGO);
module_param(exit_desc, charp, S_IRUGO);

static int m_chrdev_open(struct inode *inode, struct file *file);
static int m_chrdev_release(struct inode *inode, struct file *file);
static long m_chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t m_chrdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t m_chrdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

/* initialize file_operations */
static const struct file_operations m_chrdev_fops = {
    .owner      = THIS_MODULE,
    .open       = m_chrdev_open,
    .release    = m_chrdev_release,
    .unlocked_ioctl = m_chrdev_ioctl,
    .read       = m_chrdev_read,
    .write       = m_chrdev_write
};

/* device data holder with DMA resources */
struct m_chr_device_data {
    struct cdev cdev;
    struct device *dev;

    /* Coherent DMA buffers */
    void *coherent_buf;
    dma_addr_t coherent_dma;
    size_t coherent_size;

    /* Streaming DMA single mapping */
    void *single_buf;
    dma_addr_t single_dma;
    size_t single_size;
    enum dma_data_direction single_dir;
    bool single_mapped;

    /* Scatter-gather DMA */
    struct page **sg_pages;
    struct sg_table sg_table;
    int sg_nents;               /* Original number of SG entries before mapping */
    dma_addr_t sg_dma;
    enum dma_data_direction sg_dir;
    bool sg_mapped;

    /* DMA pool */
    struct dma_pool *dma_pool;
    void *pool_buf;
    dma_addr_t pool_dma;

    /* DMA mask */
    u64 dma_mask;

    /* Statistics */
    atomic_t ioctl_count;
};

/* global storage for device Major number */
static int dev_major = 0;

/* sysfs class structure */
static struct class *m_chrdev_class = NULL;

/* array of m_chr_device_data for */
static struct m_chr_device_data m_chrdev_data[MAX_DEV];

static int m_chrdev_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

/*==============================================================================
 * DMA Helper Functions
 *==============================================================================*/

/* Convert user direction to kernel DMA direction */
static enum dma_data_direction user_to_kernel_dir(int user_dir)
{
    switch (user_dir) {
    case DMA_USER_TO_DEVICE:
        return DMA_TO_DEVICE;
    case DMA_USER_FROM_DEVICE:
        return DMA_FROM_DEVICE;
    case DMA_USER_BIDIRECTIONAL:
        return DMA_BIDIRECTIONAL;
    default:
        return DMA_NONE;
    }
}

/*==============================================================================
 * Coherent DMA Operations
 *==============================================================================*/

static int dma_alloc_coherent_dev(struct device *dev, struct m_chr_device_data *dd,
                                   size_t size)
{
    printk(KERN_INFO "DMA: Allocating coherent buffer, size: %zu\n", size);

    if (dd->coherent_buf) {
        printk(KERN_WARNING "DMA: Coherent buffer already allocated\n");
        return -EBUSY;
    }

    dd->coherent_buf = dma_alloc_coherent(dev, size, &dd->coherent_dma,
                                          GFP_KERNEL);
    if (!dd->coherent_buf) {
        printk(KERN_ERR "DMA: Failed to allocate coherent buffer\n");
        return -ENOMEM;
    }

    dd->coherent_size = size;

    /* Initialize buffer with pattern */
    memset(dd->coherent_buf, 0xAA, size);

    printk(KERN_INFO "DMA: Coherent buffer allocated\n");
    printk(KERN_INFO "DMA:   virt addr = %p\n", dd->coherent_buf);
    printk(KERN_INFO "DMA:   dma addr  = %#llx\n", (u64)dd->coherent_dma);

    return 0;
}

static int dma_free_coherent_dev(struct device *dev, struct m_chr_device_data *dd)
{
    if (!dd->coherent_buf) {
        printk(KERN_WARNING "DMA: No coherent buffer to free\n");
        return -EINVAL;
    }

    printk(KERN_INFO "DMA: Freeing coherent buffer\n");
    dma_free_coherent(dev, dd->coherent_size, dd->coherent_buf, dd->coherent_dma);

    dd->coherent_buf = NULL;
    dd->coherent_dma = 0;
    dd->coherent_size = 0;

    return 0;
}

static int dma_read_coherent(struct m_chr_device_data *dd,
                              struct dma_ioctl_param __user *uparam)
{
    struct dma_ioctl_param param;
    size_t copy_size;

    if (!dd->coherent_buf) {
        printk(KERN_WARNING "DMA: No coherent buffer allocated\n");
        return -EINVAL;
    }

    if (copy_from_user(&param, uparam, sizeof(param))) {
        return -EFAULT;
    }

    copy_size = min(param.size, dd->coherent_size);

    if (copy_to_user((void __user *)param.user_addr, dd->coherent_buf, copy_size)) {
        return -EFAULT;
    }

    param.size = copy_size;
    param.result = 0;

    if (copy_to_user(uparam, &param, sizeof(param))) {
        return -EFAULT;
    }

    printk(KERN_INFO "DMA: Read %zu bytes from coherent buffer to user\n", copy_size);

    return 0;
}

static int dma_write_coherent(struct m_chr_device_data *dd,
                               struct dma_ioctl_param __user *uparam)
{
    struct dma_ioctl_param param;
    size_t copy_size;

    if (!dd->coherent_buf) {
        printk(KERN_WARNING "DMA: No coherent buffer allocated\n");
        return -EINVAL;
    }

    if (copy_from_user(&param, uparam, sizeof(param))) {
        return -EFAULT;
    }

    copy_size = min(param.size, dd->coherent_size);

    if (copy_from_user(dd->coherent_buf, (void __user *)param.user_addr, copy_size)) {
        return -EFAULT;
    }

    param.size = copy_size;
    param.result = 0;

    if (copy_to_user(uparam, &param, sizeof(param))) {
        return -EFAULT;
    }

    printk(KERN_INFO "DMA: Wrote %zu bytes from user to coherent buffer\n", copy_size);

    return 0;
}

/*==============================================================================
 * Streaming DMA Single Mapping Operations
 *==============================================================================*/

static int dma_map_single_dev(struct device *dev, struct m_chr_device_data *dd,
                               unsigned long size, int direction)
{
    enum dma_data_direction dir = user_to_kernel_dir(direction);

    printk(KERN_INFO "DMA: Mapping single buffer, size: %lu, dir: %d\n", size, dir);

    if (dd->single_mapped) {
        printk(KERN_WARNING "DMA: Single buffer already mapped\n");
        return -EBUSY;
    }

    dd->single_buf = kmalloc(size, GFP_KERNEL);
    if (!dd->single_buf) {
        printk(KERN_ERR "DMA: Failed to allocate single buffer\n");
        return -ENOMEM;
    }

    dd->single_dma = dma_map_single(dev, dd->single_buf, size, dir);
    if (dma_mapping_error(dev, dd->single_dma)) {
        printk(KERN_ERR "DMA: Failed to map single buffer\n");
        kfree(dd->single_buf);
        dd->single_buf = NULL;
        return -EIO;
    }

    dd->single_size = size;
    dd->single_dir = dir;
    dd->single_mapped = true;

    printk(KERN_INFO "DMA: Single buffer mapped\n");
    printk(KERN_INFO "DMA:   virt addr = %p\n", dd->single_buf);
    printk(KERN_INFO "DMA:   dma addr  = %#llx\n", (u64)dd->single_dma);

    return 0;
}

static int dma_unmap_single_dev(struct device *dev, struct m_chr_device_data *dd)
{
    if (!dd->single_mapped) {
        printk(KERN_WARNING "DMA: No single buffer to unmap\n");
        return -EINVAL;
    }

    printk(KERN_INFO "DMA: Unmapping single buffer\n");
    dma_unmap_single(dev, dd->single_dma, dd->single_size, dd->single_dir);
    kfree(dd->single_buf);

    dd->single_buf = NULL;
    dd->single_dma = 0;
    dd->single_size = 0;
    dd->single_mapped = false;

    return 0;
}

static int dma_sync_single_dev(struct device *dev, struct m_chr_device_data *dd,
                                int direction)
{
    enum dma_data_direction dir = user_to_kernel_dir(direction);

    if (!dd->single_mapped) {
        printk(KERN_WARNING "DMA: No single buffer to sync\n");
        return -EINVAL;
    }

    printk(KERN_INFO "DMA: Syncing single buffer, dir: %d\n", dir);

    if (dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL) {
        dma_sync_single_for_device(dev, dd->single_dma, dd->single_size, dir);
        printk(KERN_INFO "DMA: Synced for device\n");
    }

    if (dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL) {
        dma_sync_single_for_cpu(dev, dd->single_dma, dd->single_size, dir);
        printk(KERN_INFO "DMA: Synced for CPU\n");
    }

    return 0;
}

/*==============================================================================
 * Scatter-Gather DMA Operations
 *==============================================================================*/

static int dma_map_sg_dev(struct device *dev, struct m_chr_device_data *dd,
                           int nents, int direction)
{
    enum dma_data_direction dir = user_to_kernel_dir(direction);
    int i, ret;

    printk(KERN_INFO "DMA: Mapping scatter-gather, nents: %d, dir: %d\n", nents, dir);

    if (dd->sg_mapped) {
        printk(KERN_WARNING "DMA: SG already mapped\n");
        return -EBUSY;
    }

    /* Allocate pages for scatter-gather */
    dd->sg_pages = kmalloc_array(nents, sizeof(struct page *), GFP_KERNEL);
    if (!dd->sg_pages) {
        printk(KERN_ERR "DMA: Failed to allocate sg_pages array\n");
        return -ENOMEM;
    }

    for (i = 0; i < nents; i++) {
        /* GFP_DMA32 ensures allocation is below 4GB on x86_64 */
        dd->sg_pages[i] = alloc_page(GFP_KERNEL | GFP_DMA32);
        if (!dd->sg_pages[i]) {
            printk(KERN_ERR "DMA: Failed to allocate page %d\n", i);
            /* Free previously allocated pages */
            while (--i >= 0) {
                __free_page(dd->sg_pages[i]);
            }
            kfree(dd->sg_pages);
            dd->sg_pages = NULL;
            return -ENOMEM;
        }
        /* Initialize page with pattern */
        memset(page_address(dd->sg_pages[i]), 0xBB, PAGE_SIZE);
    }

    /* Initialize scatter-gather table */
    ret = sg_alloc_table(&dd->sg_table, nents, GFP_KERNEL);
    if (ret) {
        printk(KERN_ERR "DMA: Failed to allocate sg table\n");
        for (i = 0; i < nents; i++) {
            __free_page(dd->sg_pages[i]);
        }
        kfree(dd->sg_pages);
        dd->sg_pages = NULL;
        return ret;
    }

    /* Set up scatter-gather entries using sg_assign_page */
    {
        struct scatterlist *sg;
        int i;
        for_each_sg(dd->sg_table.sgl, sg, nents, i) {
            sg_assign_page(sg, dd->sg_pages[i]);
            sg->length = PAGE_SIZE;
        }
    }

    /* Map scatter-gather */
    ret = dma_map_sg(dev, dd->sg_table.sgl, nents, dir);
    if (ret == 0) {
        printk(KERN_ERR "DMA: Failed to map sg\n");
        sg_free_table(&dd->sg_table);
        for (i = 0; i < nents; i++) {
            __free_page(dd->sg_pages[i]);
        }
        kfree(dd->sg_pages);
        dd->sg_pages = NULL;
        return -EIO;
    }

    /* Save the original nents before it gets modified by dma_map_sg */
    dd->sg_nents = nents;
    dd->sg_dir = dir;
    dd->sg_mapped = true;

    printk(KERN_INFO "DMA: SG mapped, %d entries\n", ret);
    {
        struct scatterlist *sg;
        int i;
        for_each_sg(dd->sg_table.sgl, sg, ret, i) {
            phys_addr_t phys_addr = sg_phys(sg);
            dma_addr_t dma_addr = sg_dma_address(sg);
            unsigned int sg_len = sg_dma_len(sg);

            printk(KERN_INFO "DMA:   sg[%d]: phys_addr=0x%pa (below 4G: %s), dma_addr/iova=0x%pad, len=0x%x\n",
                   i, &phys_addr, phys_addr < SZ_4G ? "yes" : "no",
                   &dma_addr, sg_len);
        }
    }

    return 0;
}

static int dma_unmap_sg_dev(struct device *dev, struct m_chr_device_data *dd)
{
    int i;

    if (!dd->sg_mapped) {
        printk(KERN_WARNING "DMA: No SG to unmap\n");
        return -EINVAL;
    }

    printk(KERN_INFO "DMA: Unmapping scatter-gather\n");

    /* Use the mapped nents from sg_table for unmap */
    dma_unmap_sg(dev, dd->sg_table.sgl, dd->sg_table.nents, dd->sg_dir);

    sg_free_table(&dd->sg_table);

    /* Use the original nents for freeing pages */
    for (i = 0; i < dd->sg_nents; i++) {
        __free_page(dd->sg_pages[i]);
    }
    kfree(dd->sg_pages);
    dd->sg_pages = NULL;

    dd->sg_mapped = false;

    return 0;
}

static int dma_sync_sg_dev(struct device *dev, struct m_chr_device_data *dd,
                           int direction)
{
    enum dma_data_direction dir = user_to_kernel_dir(direction);

    if (!dd->sg_mapped) {
        printk(KERN_WARNING "DMA: No SG to sync\n");
        return -EINVAL;
    }

    printk(KERN_INFO "DMA: Syncing scatter-gather, dir: %d\n", dir);

    /* Use the mapped nents from sg_table for sync */
    if (dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL) {
        dma_sync_sg_for_device(dev, dd->sg_table.sgl, dd->sg_table.nents, dir);
        printk(KERN_INFO "DMA: Synced SG for device\n");
    }

    if (dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL) {
        dma_sync_sg_for_cpu(dev, dd->sg_table.sgl, dd->sg_table.nents, dir);
        printk(KERN_INFO "DMA: Synced SG for CPU\n");
    }

    return 0;
}

/*==============================================================================
 * DMA Pool Operations
 *==============================================================================*/

static int dma_pool_create_dev(struct device *dev, struct m_chr_device_data *dd)
{
    printk(KERN_INFO "DMA: Creating DMA pool\n");

    if (dd->dma_pool) {
        printk(KERN_WARNING "DMA: Pool already created\n");
        return -EBUSY;
    }

    dd->dma_pool = dma_pool_create("demo_dma_pool", dev, DMA_POOL_SIZE,
                                    DMA_POOL_BOUNDARY, 0);
    if (!dd->dma_pool) {
        printk(KERN_ERR "DMA: Failed to create pool\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "DMA: Pool created, size: %d\n", DMA_POOL_SIZE);

    return 0;
}

static int dma_pool_alloc_dev(struct device *dev, struct m_chr_device_data *dd)
{
    printk(KERN_INFO "DMA: Allocating from pool\n");

    if (!dd->dma_pool) {
        printk(KERN_WARNING "DMA: Pool not created\n");
        return -EINVAL;
    }

    if (dd->pool_buf) {
        printk(KERN_WARNING "DMA: Pool buffer already allocated\n");
        return -EBUSY;
    }

    dd->pool_buf = dma_pool_alloc(dd->dma_pool, GFP_KERNEL, &dd->pool_dma);
    if (!dd->pool_buf) {
        printk(KERN_ERR "DMA: Failed to allocate from pool\n");
        return -ENOMEM;
    }

    /* Initialize buffer */
    memset(dd->pool_buf, 0xCC, DMA_POOL_SIZE);

    printk(KERN_INFO "DMA: Allocated from pool\n");
    printk(KERN_INFO "DMA:   virt addr = %p\n", dd->pool_buf);
    printk(KERN_INFO "DMA:   dma addr  = %#llx\n", (u64)dd->pool_dma);

    return 0;
}

static int dma_pool_free_dev(struct device *dev, struct m_chr_device_data *dd)
{
    if (!dd->pool_buf || !dd->dma_pool) {
        printk(KERN_WARNING "DMA: No pool buffer to free\n");
        return -EINVAL;
    }

    printk(KERN_INFO "DMA: Freeing pool buffer\n");
    dma_pool_free(dd->dma_pool, dd->pool_buf, dd->pool_dma);

    dd->pool_buf = NULL;
    dd->pool_dma = 0;

    return 0;
}

static int dma_pool_destroy_dev(struct device *dev, struct m_chr_device_data *dd)
{
    if (!dd->dma_pool) {
        printk(KERN_WARNING "DMA: No pool to destroy\n");
        return -EINVAL;
    }

    if (dd->pool_buf) {
        printk(KERN_WARNING "DMA: Pool buffer still allocated, freeing\n");
        dma_pool_free(dd->dma_pool, dd->pool_buf, dd->pool_dma);
        dd->pool_buf = NULL;
        dd->pool_dma = 0;
    }

    printk(KERN_INFO "DMA: Destroying pool\n");
    dma_pool_destroy(dd->dma_pool);
    dd->dma_pool = NULL;

    return 0;
}

/*==============================================================================
 * DMA Information and Mask Configuration
 *==============================================================================*/

static int dma_get_info(struct m_chr_device_data *dd,
                        struct dma_ioctl_param __user *uparam)
{
    struct dma_ioctl_param param;

    if (copy_from_user(&param, uparam, sizeof(param))) {
        return -EFAULT;
    }

    param.dma_addr = 0;
    param.result = 0;

    /* Coherent DMA info */
    if (dd->coherent_buf) {
        param.dma_addr = dd->coherent_dma;
        param.size = dd->coherent_size;
        printk(KERN_INFO "DMA: Coherent: %#llx, size: %zu\n",
               (u64)dd->coherent_dma, dd->coherent_size);
    }

    /* Single mapping info */
    if (dd->single_mapped) {
        param.dma_addr = dd->single_dma;
        param.size = dd->single_size;
        printk(KERN_INFO "DMA: Single: %#llx, size: %zu\n",
               (u64)dd->single_dma, dd->single_size);
    }

    /* SG info */
    if (dd->sg_mapped) {
        param.count = dd->sg_table.nents;
        param.dma_addr = sg_dma_address(dd->sg_table.sgl);
        printk(KERN_INFO "DMA: SG: %#llx, nents: %d\n",
               (u64)sg_dma_address(dd->sg_table.sgl), dd->sg_table.nents);
    }

    /* Pool info */
    if (dd->pool_buf) {
        param.dma_addr = dd->pool_dma;
        param.size = DMA_POOL_SIZE;
        printk(KERN_INFO "DMA: Pool: %#llx, size: %d\n",
               (u64)dd->pool_dma, DMA_POOL_SIZE);
    }

    /* DMA mask info */
    param.mask_bits = (dd->dma_mask == DMA_BIT_MASK(64)) ? 64 : 32;
    printk(KERN_INFO "DMA: DMA mask: %u bits\n", param.mask_bits);

    if (copy_to_user(uparam, &param, sizeof(param))) {
        return -EFAULT;
    }

    return 0;
}

static int m_chrdev_dma_set_mask(struct device *dev, struct m_chr_device_data *dd,
                                  unsigned int mask_bits)
{
    u64 mask;
    int ret;

    printk(KERN_INFO "DMA: Setting mask to %u bits\n", mask_bits);

    mask = (mask_bits == 64) ? DMA_BIT_MASK(64) : DMA_BIT_MASK(32);

    ret = dma_set_mask_and_coherent(dev, mask);
    if (ret) {
        printk(KERN_ERR "DMA: Failed to set mask\n");
        return ret;
    }

    dd->dma_mask = mask;
    printk(KERN_INFO "DMA: DMA mask set to %#llx\n", mask);

    return 0;
}

/*==============================================================================
 * Device Operations
 *==============================================================================*/

static int m_chrdev_open(struct inode *inode, struct file *file)
{
    struct m_chr_device_data *dd;
    int minor = MINOR(inode->i_rdev);

    printk(KERN_INFO "DMA: Device open, minor: %d\n", minor);

    if (minor >= MAX_DEV) {
        return -ENODEV;
    }

    dd = &m_chrdev_data[minor];
    file->private_data = dd;
    atomic_inc(&dd->ioctl_count);

    return 0;
}

static int m_chrdev_release(struct inode *inode, struct file *file)
{
    struct m_chr_device_data *dd = file->private_data;

    printk(KERN_INFO "DMA: Device close\n");

    atomic_dec(&dd->ioctl_count);

    return 0;
}

static long m_chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct m_chr_device_data *dd = file->private_data;
    struct device *dev = dd->dev;
    struct dma_ioctl_param param;
    int ret = 0;
    void __user *argp = (void __user *)arg;

    printk(KERN_INFO "DMA: IOCTL cmd: %u\n", cmd);

    if (_IOC_TYPE(cmd) != DMA_MAGIC) {
        return -ENOTTY;
    }

    switch (cmd) {
    case DMA_IOCTL_ALLOC_COHERENT:
        if (copy_from_user(&param, argp, sizeof(param))) {
            return -EFAULT;
        }
        ret = dma_alloc_coherent_dev(dev, dd, param.size);
        if (ret == 0) {
            param.dma_addr = dd->coherent_dma;
            param.result = 0;
            if (copy_to_user(argp, &param, sizeof(param))) {
                return -EFAULT;
            }
        }
        break;

    case DMA_IOCTL_FREE_COHERENT:
        ret = dma_free_coherent_dev(dev, dd);
        break;

    case DMA_IOCTL_READ_COHERENT:
        ret = dma_read_coherent(dd, argp);
        break;

    case DMA_IOCTL_WRITE_COHERENT:
        ret = dma_write_coherent(dd, argp);
        break;

    case DMA_IOCTL_MAP_SINGLE:
        if (copy_from_user(&param, argp, sizeof(param))) {
            return -EFAULT;
        }
        ret = dma_map_single_dev(dev, dd, param.size, param.direction);
        if (ret == 0) {
            param.dma_addr = dd->single_dma;
            param.result = 0;
            if (copy_to_user(argp, &param, sizeof(param))) {
                return -EFAULT;
            }
        }
        break;

    case DMA_IOCTL_UNMAP_SINGLE:
        ret = dma_unmap_single_dev(dev, dd);
        break;

    case DMA_IOCTL_SYNC_SINGLE:
        if (copy_from_user(&param, argp, sizeof(param))) {
            return -EFAULT;
        }
        ret = dma_sync_single_dev(dev, dd, param.direction);
        break;

    case DMA_IOCTL_MAP_SG:
        if (copy_from_user(&param, argp, sizeof(param))) {
            return -EFAULT;
        }
        ret = dma_map_sg_dev(dev, dd, SG_NENTS, param.direction);
        if (ret == 0) {
            param.dma_addr = sg_dma_address(dd->sg_table.sgl);
            param.count = dd->sg_table.nents;
            param.result = 0;
            if (copy_to_user(argp, &param, sizeof(param))) {
                return -EFAULT;
            }
        }
        break;

    case DMA_IOCTL_UNMAP_SG:
        ret = dma_unmap_sg_dev(dev, dd);
        break;

    case DMA_IOCTL_SYNC_SG:
        if (copy_from_user(&param, argp, sizeof(param))) {
            return -EFAULT;
        }
        ret = dma_sync_sg_dev(dev, dd, param.direction);
        break;

    case DMA_IOCTL_POOL_CREATE:
        ret = dma_pool_create_dev(dev, dd);
        break;

    case DMA_IOCTL_POOL_ALLOC:
        ret = dma_pool_alloc_dev(dev, dd);
        if (ret == 0) {
            param.dma_addr = dd->pool_dma;
            param.size = DMA_POOL_SIZE;
            param.result = 0;
            if (copy_to_user(argp, &param, sizeof(param))) {
                return -EFAULT;
            }
        }
        break;

    case DMA_IOCTL_POOL_FREE:
        ret = dma_pool_free_dev(dev, dd);
        break;

    case DMA_IOCTL_POOL_DESTROY:
        ret = dma_pool_destroy_dev(dev, dd);
        break;

    case DMA_IOCTL_GET_INFO:
        ret = dma_get_info(dd, argp);
        break;

    case DMA_IOCTL_SET_MASK:
        if (copy_from_user(&param, argp, sizeof(param))) {
            return -EFAULT;
        }
        ret = m_chrdev_dma_set_mask(dev, dd, param.mask_bits);
        break;

    default:
        return -ENOTTY;
    }

    return ret;
}

static ssize_t m_chrdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    struct m_chr_device_data *dd = file->private_data;
    (void)dd; /* unused */
    uint8_t *data = "DMA Demo Driver - Use ioctl for DMA operations\n";
    size_t datalen = strlen(data);

    printk("DMA: Reading device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    if (count > datalen) {
        count = datalen;
    }

    if (copy_to_user(buf, data, count)) {
        return -EFAULT;
    }

    return count;
}

static ssize_t m_chrdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    struct m_chr_device_data *dd = file->private_data;
    (void)dd; /* unused */
    size_t maxdatalen = 30, ncopied;
    uint8_t databuf[30];

    printk("DMA: Writing device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    if (count < maxdatalen) {
        maxdatalen = count;
    }

    ncopied = copy_from_user(databuf, buf, maxdatalen);

    if (ncopied == 0) {
        printk("DMA: Copied %zd bytes from the user\n", maxdatalen);
    } else {
        printk("DMA: Couldn't copy %zd bytes from the user\n", ncopied);
    }

    databuf[maxdatalen] = 0;

    printk("DMA: Data from the user: %s\n", databuf);

    return count;
}

static int __init m_chr_init(void)
{
    int err, idx;
    dev_t devno;

    printk(KERN_INFO "DMA module %s init desc:%s\n", __func__, init_desc);
    printk(KERN_INFO "DMA git version:%s\n", DEMO_GIT_VERSION);

    /* Dynamically apply for device number */
    err = alloc_chrdev_region(&devno, 0, MAX_DEV, "m_chrdev");
    dev_major = MAJOR(devno);

    /* create sysfs class */
    m_chrdev_class = class_create("m_chrdev_cls");
    m_chrdev_class->dev_uevent = m_chrdev_uevent;

    /* Create necessary number of the devices */
    for (idx = 0; idx < MAX_DEV; idx++) {
        struct m_chr_device_data *dd = &m_chrdev_data[idx];

        /* Initialize device data */
        memset(dd, 0, sizeof(*dd));
        atomic_set(&dd->ioctl_count, 0);
        dd->dma_mask = DMA_BIT_MASK(64); /* Default to 64-bit */

        /* init new device */
        cdev_init(&dd->cdev, &m_chrdev_fops);
        dd->cdev.owner = THIS_MODULE;

        /* add device to the system */
        cdev_add(&dd->cdev, MKDEV(dev_major, idx), 1);

        /* create device node */
        dd->dev = device_create(m_chrdev_class, NULL, MKDEV(dev_major, idx),
                                 NULL, "m_chrdev_%d", idx);
        if (IS_ERR(dd->dev)) {
            printk(KERN_ERR "DMA: Failed to create device %d\n", idx);
            continue;
        }

        /*
         * For virtual/character devices, we need to set the dma_mask pointer
         * explicitly. The device_create() doesn't set this up for us.
         * This must be done BEFORE calling any DMA API functions.
         */
        dd->dev->dma_mask = &dd->dma_mask;
        dd->dev->coherent_dma_mask = dd->dma_mask;

        printk(KERN_INFO "DMA: Device %d created\n", idx);
    }

    printk(KERN_INFO "DMA: Module initialized, %d devices\n", MAX_DEV);

    return 0;
}

/* 模块卸载函数 */
static void __exit m_chr_exit(void)
{
    int idx;

    printk(KERN_INFO "DMA module %s exit desc:%s\n", __func__, exit_desc);

    for (idx = 0; idx < MAX_DEV; idx++) {
        struct m_chr_device_data *dd = &m_chrdev_data[idx];

        /* Clean up any remaining DMA resources */
        if (dd->coherent_buf) {
            dma_free_coherent(dd->dev, dd->coherent_size, dd->coherent_buf,
                              dd->coherent_dma);
        }

        if (dd->single_mapped) {
            dma_unmap_single(dd->dev, dd->single_dma, dd->single_size, dd->single_dir);
            kfree(dd->single_buf);
        }

        if (dd->sg_mapped) {
            int i;
            dma_unmap_sg(dd->dev, dd->sg_table.sgl, dd->sg_table.nents, dd->sg_dir);
            sg_free_table(&dd->sg_table);
            /* Use the original nents to free all allocated pages */
            for (i = 0; i < dd->sg_nents; i++) {
                __free_page(dd->sg_pages[i]);
            }
            kfree(dd->sg_pages);
        }

        if (dd->pool_buf) {
            dma_pool_free(dd->dma_pool, dd->pool_buf, dd->pool_dma);
        }

        if (dd->dma_pool) {
            dma_pool_destroy(dd->dma_pool);
        }

        device_destroy(m_chrdev_class, MKDEV(dev_major, idx));
    }

    class_destroy(m_chrdev_class);
    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

    printk(KERN_INFO "DMA: Module exited\n");

    return;
}

module_init(m_chr_init);
module_exit(m_chr_exit);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lhj <872648180@qq.com>");
MODULE_DESCRIPTION("DMA demo for learning");
MODULE_ALIAS("dma demo");
MODULE_VERSION(DEMO_GIT_VERSION);
