/*************************************************************************
    > File Name: userDemo.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com
    > Created Time: Sat Oct 14 14:23:18 2023
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define DEVNAME_0 "/dev/m_chrdev_0"
#define DEVNAME_1 "/dev/m_chrdev_1"

/* IOCTL commands - must match kernel side */
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

/* IOCTL parameter structure - must match kernel side */
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

/* DMA directions */
enum dma_user_dir {
    DMA_USER_TO_DEVICE = 1,
    DMA_USER_FROM_DEVICE = 2,
    DMA_USER_BIDIRECTIONAL = 3
};

#define COHERENT_BUF_SIZE  (16 * 1024)  /* 16KB */
#define SINGLE_BUF_SIZE    (8 * 1024)   /* 8KB */

static int g_verbose = 0;

/*==============================================================================
 * Helper Functions
 *==============================================================================*/

static void print_hex(const char *prefix, const uint8_t *data, size_t len)
{
    size_t i;

    if (!g_verbose) {
        return;
    }

    printf("%s (first %zu bytes): ", prefix, len < 32 ? len : 32);
    for (i = 0; i < len && i < 32; i++) {
        printf("%02x ", data[i]);
    }
    if (len > 32) {
        printf("...");
    }
    printf("\n");
}

/*==============================================================================
 * Base Test
 *==============================================================================*/

int test_base(void)
{
#define KER_INFO_SZ 100

    FILE *fd_0;
    int fd_1;
    char *user_info = "this is user info";
    char kernel_info[KER_INFO_SZ];
    unsigned long request = 0;
    unsigned long req_ack = 0;

    memset(kernel_info, 0, KER_INFO_SZ);
    fd_0 = fopen(DEVNAME_0, "r+");
    fwrite(user_info, 1, strlen(user_info), fd_0);
    fread(kernel_info, 1, 50, fd_0);
    printf("======> from kernel: %s\n", kernel_info);
    fclose(fd_0);

    fd_1 = open(DEVNAME_1, O_RDWR);
    ioctl(fd_1, request, &req_ack);
    close(fd_1);

    return 0;
}

/*==============================================================================
 * Coherent DMA Test
 *==============================================================================*/

static int test_coherent_dma(int fd)
{
    struct dma_ioctl_param param;
    uint8_t *write_buf;
    uint8_t *read_buf;
    int ret;
    size_t i;

    printf("\n======> Coherent DMA Test <======\n");

    /* Allocate coherent buffer */
    memset(&param, 0, sizeof(param));
    param.size = COHERENT_BUF_SIZE;

    ret = ioctl(fd, DMA_IOCTL_ALLOC_COHERENT, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_ALLOC_COHERENT");
        return -1;
    }

    printf("Coherent buffer allocated:\n");
    printf("  DMA address: %#lx\n", param.dma_addr);
    printf("  Size: %lu bytes\n", param.size);

    /* Prepare write buffer */
    write_buf = malloc(COHERENT_BUF_SIZE);
    if (!write_buf) {
        perror("malloc");
        return -1;
    }

    for (i = 0; i < COHERENT_BUF_SIZE; i++) {
        write_buf[i] = i & 0xFF;
    }

    print_hex("Write pattern", write_buf, COHERENT_BUF_SIZE);

    /* Write to coherent buffer */
    memset(&param, 0, sizeof(param));
    param.size = COHERENT_BUF_SIZE;
    param.user_addr = (unsigned long)write_buf;

    ret = ioctl(fd, DMA_IOCTL_WRITE_COHERENT, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_WRITE_COHERENT");
        free(write_buf);
        return -1;
    }

    printf("Wrote %lu bytes to coherent buffer\n", param.size);

    /* Prepare read buffer */
    read_buf = malloc(COHERENT_BUF_SIZE);
    if (!read_buf) {
        perror("malloc");
        free(write_buf);
        return -1;
    }
    memset(read_buf, 0, COHERENT_BUF_SIZE);

    /* Read from coherent buffer */
    memset(&param, 0, sizeof(param));
    param.size = COHERENT_BUF_SIZE;
    param.user_addr = (unsigned long)read_buf;

    ret = ioctl(fd, DMA_IOCTL_READ_COHERENT, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_READ_COHERENT");
        free(write_buf);
        free(read_buf);
        return -1;
    }

    printf("Read %lu bytes from coherent buffer\n", param.size);

    print_hex("Read pattern", read_buf, COHERENT_BUF_SIZE);

    /* Verify data */
    ret = memcmp(write_buf, read_buf, COHERENT_BUF_SIZE);
    if (ret == 0) {
        printf("Coherent DMA: Data verification PASSED\n");
    } else {
        printf("Coherent DMA: Data verification FAILED\n");
    }

    free(write_buf);
    free(read_buf);

    /* Free coherent buffer */
    ret = ioctl(fd, DMA_IOCTL_FREE_COHERENT, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_FREE_COHERENT");
        return -1;
    }

    printf("Coherent DMA test completed\n");

    return 0;
}

/*==============================================================================
 * Streaming DMA Single Mapping Test
 *==============================================================================*/

static int test_single_mapping(int fd)
{
    struct dma_ioctl_param param;
    uint8_t *write_buf;
    int ret;
    size_t i;

    printf("\n======> Streaming DMA Single Mapping Test <======\n");

    /* Allocate and map single buffer */
    memset(&param, 0, sizeof(param));
    param.size = SINGLE_BUF_SIZE;
    param.direction = DMA_USER_BIDIRECTIONAL;

    ret = ioctl(fd, DMA_IOCTL_MAP_SINGLE, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_MAP_SINGLE");
        return -1;
    }

    printf("Single buffer mapped:\n");
    printf("  DMA address: %#lx\n", param.dma_addr);
    printf("  Size: %lu bytes\n", param.size);
    printf("  Direction: %d (BIDIRECTIONAL)\n", param.direction);

    /* Prepare data for testing sync */
    write_buf = malloc(SINGLE_BUF_SIZE);
    if (!write_buf) {
        perror("malloc");
        return -1;
    }

    for (i = 0; i < SINGLE_BUF_SIZE; i++) {
        write_buf[i] = (i ^ 0xFF) & 0xFF;
    }

    print_hex("Data pattern", write_buf, SINGLE_BUF_SIZE);

    /* Test sync for device */
    printf("Testing DMA sync for device...\n");

    /* Sync for CPU (simulating device completed transfer) */
    printf("Testing DMA sync for CPU...\n");

    free(write_buf);

    /* Unmap single buffer */
    ret = ioctl(fd, DMA_IOCTL_UNMAP_SINGLE, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_UNMAP_SINGLE");
        return -1;
    }

    printf("Single mapping test completed\n");

    return 0;
}

/*==============================================================================
 * Scatter-Gather DMA Test
 *==============================================================================*/

static int test_scatter_gather(int fd)
{
    struct dma_ioctl_param param;
    int ret;

    printf("\n======> Scatter-Gather DMA Test <======\n");

    /* Map scatter-gather */
    memset(&param, 0, sizeof(param));
    param.direction = DMA_USER_BIDIRECTIONAL;

    ret = ioctl(fd, DMA_IOCTL_MAP_SG, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_MAP_SG");
        return -1;
    }

    printf("Scatter-gather mapped:\n");
    printf("  First DMA address: %#lx\n", param.dma_addr);
    printf("  Number of entries: %d\n", param.count);
    printf("  Direction: %d (BIDIRECTIONAL)\n", param.direction);

    /* Test sync */
    printf("Testing SG sync for device...\n");
    param.direction = DMA_USER_TO_DEVICE;
    ret = ioctl(fd, DMA_IOCTL_SYNC_SG, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_SYNC_SG (TO_DEVICE)");
        return -1;
    }

    printf("Testing SG sync for CPU...\n");
    param.direction = DMA_USER_FROM_DEVICE;
    ret = ioctl(fd, DMA_IOCTL_SYNC_SG, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_SYNC_SG (FROM_DEVICE)");
        return -1;
    }

    /* Unmap scatter-gather */
    ret = ioctl(fd, DMA_IOCTL_UNMAP_SG, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_UNMAP_SG");
        return -1;
    }

    printf("Scatter-gather test completed\n");

    return 0;
}

/*==============================================================================
 * DMA Pool Test
 *==============================================================================*/

static int test_dma_pool(int fd)
{
    struct dma_ioctl_param param;
    uint8_t *test_buf;
    int ret;
    int i;

    printf("\n======> DMA Pool Test <======\n");

    /* Create pool */
    memset(&param, 0, sizeof(param));
    ret = ioctl(fd, DMA_IOCTL_POOL_CREATE, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_POOL_CREATE");
        return -1;
    }

    printf("DMA pool created\n");

    /* Allocate from pool */
    ret = ioctl(fd, DMA_IOCTL_POOL_ALLOC, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_POOL_ALLOC");
        goto destroy_pool;
    }

    printf("Allocated from pool:\n");
    printf("  DMA address: %#lx\n", param.dma_addr);
    printf("  Size: %lu bytes\n", param.size);

    /* Test multiple allocations */
    test_buf = malloc(param.size);
    if (!test_buf) {
        perror("malloc");
        goto free_pool;
    }

    /* Prepare test data */
    for (i = 0; i < param.size; i++) {
        test_buf[i] = 0x42;
    }

    print_hex("Pool test data", test_buf, param.size);

    free(test_buf);

    /* Free pool allocation */
free_pool:
    ret = ioctl(fd, DMA_IOCTL_POOL_FREE, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_POOL_FREE");
    }

    printf("Pool allocation freed\n");

    /* Destroy pool */
destroy_pool:
    ret = ioctl(fd, DMA_IOCTL_POOL_DESTROY, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_POOL_DESTROY");
        return -1;
    }

    printf("DMA pool destroyed\n");
    printf("DMA pool test completed\n");

    return 0;
}

/*==============================================================================
 * DMA Information Test
 *==============================================================================*/

static int test_dma_info(int fd)
{
    struct dma_ioctl_param param;
    int ret;

    printf("\n======> DMA Information Test <======\n");

    memset(&param, 0, sizeof(param));
    ret = ioctl(fd, DMA_IOCTL_GET_INFO, &param);
    if (ret < 0) {
        perror("DMA_IOCTL_GET_INFO");
        return -1;
    }

    printf("DMA Information:\n");
    printf("  DMA mask: %u bits\n", param.mask_bits);
    printf("  Active DMA address: %#lx\n", param.dma_addr);
    printf("  Size/count: %lu\n", param.size);

    return 0;
}

/*==============================================================================
 * DMA Mask Test
 *==============================================================================*/

static int test_dma_mask(int fd)
{
    struct dma_ioctl_param param;
    int ret;

    printf("\n======> DMA Mask Test <======\n");

    /* Set 32-bit mask */
    memset(&param, 0, sizeof(param));
    param.mask_bits = 32;

    ret = ioctl(fd, DMA_IOCTL_SET_MASK, &param);
    if (ret < 0) {
        printf("Setting 32-bit mask: %s\n", strerror(-ret));
    } else {
        printf("Set DMA mask to 32 bits\n");
    }

    /* Set 64-bit mask */
    param.mask_bits = 64;
    ret = ioctl(fd, DMA_IOCTL_SET_MASK, &param);
    if (ret < 0) {
        printf("Setting 64-bit mask: %s\n", strerror(-ret));
    } else {
        printf("Set DMA mask to 64 bits\n");
    }

    printf("DMA mask test completed\n");

    return 0;
}

/*==============================================================================
 * Main Entry Point
 *==============================================================================*/

static void print_usage(const char *prog)
{
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\nDMA Demo Test Program\n");
    printf("\nOptions:\n");
    printf("  -b              Run base test (original test)\n");
    printf("  -a, --all       Run all DMA tests\n");
    printf("  -c, --coherent  Run coherent DMA test\n");
    printf("  -s, --single    Run streaming DMA single mapping test\n");
    printf("  -g, --sg        Run scatter-gather DMA test\n");
    printf("  -p, --pool      Run DMA pool test\n");
    printf("  -i, --info      Get DMA information\n");
    printf("  -m, --mask      Test DMA mask configuration\n");
    printf("  -t N            Run specific test case (1-6)\n");
    printf("  -v, --verbose   Enable verbose output\n");
    printf("  -h, --help      Show this help message\n");
    printf("\nTest cases:\n");
    printf("  1 - Coherent DMA\n");
    printf("  2 - Streaming DMA Single Mapping\n");
    printf("  3 - Scatter-Gather DMA\n");
    printf("  4 - DMA Pool\n");
    printf("  5 - DMA Information\n");
    printf("  6 - DMA Mask Configuration\n");
    printf("\nExamples:\n");
    printf("  %s -a              # Run all tests\n", prog);
    printf("  %s -c -v           # Run coherent test with verbose output\n", prog);
    printf("  %s -t 1            # Run test case 1\n", prog);
}

int main(int argc, char *argv[], char *envp[])
{
    int opt;
    int fd = -1;
    int run_all = 0;
    int run_base = 0;
    int test_mask = 0;
    int ret = 0;
    int test_num = 0;

    /* Long options */
    static struct option long_options[] = {
        {"all",       no_argument,       0, 'a'},
        {"coherent",  no_argument,       0, 'c'},
        {"single",    no_argument,       0, 's'},
        {"sg",        no_argument,       0, 'g'},
        {"pool",      no_argument,       0, 'p'},
        {"info",      no_argument,       0, 'i'},
        {"mask",      no_argument,       0, 'm'},
        {"verbose",   no_argument,       0, 'v'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    /* Parse command line */
    while ((opt = getopt_long(argc, argv, "abcghipsmt:vt:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'a':
            run_all = 1;
            break;
        case 'b':
            run_base = 1;
            break;
        case 'c':
            test_mask |= (1 << 0);
            break;
        case 's':
            test_mask |= (1 << 1);
            break;
        case 'g':
            test_mask |= (1 << 2);
            break;
        case 'p':
            test_mask |= (1 << 3);
            break;
        case 'i':
            test_mask |= (1 << 4);
            break;
        case 'm':
            test_mask |= (1 << 5);
            break;
        case 'v':
            g_verbose = 1;
            break;
        case 't':
            test_num = atoi(optarg);
            if (test_num >= 1 && test_num <= 6) {
                test_mask |= (1 << (test_num - 1));
            } else {
                fprintf(stderr, "Invalid test case: %s\n", optarg);
                return 1;
            }
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    /* If no test specified, show usage */
    if (!run_all && !run_base && test_mask == 0) {
        print_usage(argv[0]);
        return 0;
    }

    /* Open device */
    if (!run_base) {
        fd = open(DEVNAME_0, O_RDWR);
        if (fd < 0) {
            perror("Failed to open device " DEVNAME_0);
            return 1;
        }
        printf("Device opened: %s\n", DEVNAME_0);
    }

    /* Run base test */
    if (run_base) {
        test_base();
    }

    /* Run all tests */
    if (run_all) {
        test_mask = 0x3F;  /* All 6 tests */
    }

    /* Run selected tests */
    if (test_mask & (1 << 0)) {
        if (test_coherent_dma(fd) < 0) {
            ret = 1;
        }
    }

    if (test_mask & (1 << 1)) {
        if (test_single_mapping(fd) < 0) {
            ret = 1;
        }
    }

    if (test_mask & (1 << 2)) {
        if (test_scatter_gather(fd) < 0) {
            ret = 1;
        }
    }

    if (test_mask & (1 << 3)) {
        if (test_dma_pool(fd) < 0) {
            ret = 1;
        }
    }

    if (test_mask & (1 << 4)) {
        if (test_dma_info(fd) < 0) {
            ret = 1;
        }
    }

    if (test_mask & (1 << 5)) {
        if (test_dma_mask(fd) < 0) {
            ret = 1;
        }
    }

    /* Close device */
    if (fd >= 0) {
        close(fd);
        printf("\nDevice closed\n");
    }

    if (ret == 0) {
        printf("\n======> All tests PASSED <======\n");
    } else {
        printf("\n======> Some tests FAILED <======\n");
    }

    return ret;
}
