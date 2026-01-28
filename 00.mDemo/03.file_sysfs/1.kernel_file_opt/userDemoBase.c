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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define DEVNAME_0 "/dev/m_chrdev_0"
#define DEVNAME_1 "/dev/m_chrdev_1"

int test_base()
{
    FILE *fd_0;
    /* int fd_1; */
    char *user_info = "this is user info";
    char kernel_info[100];
    /* unsigned long request = 0; */
    /* unsigned long req_ack = 0; */

    fd_0 = fopen(DEVNAME_0, "r+");
    fwrite(user_info, 1, strlen(user_info), fd_0);
    fread(kernel_info, 1, 50, fd_0);
    /* printf("======> from kernel: %s\n", kernel_info); */
    fclose(fd_0);

    /* fd_1 = open(DEVNAME_1, O_RDWR); */
    /* ioctl(fd_1, request, &req_ack); */
    /* close(fd_1); */

    return 0;
}

int test_cases(char *test_case)
{
    switch (*test_case) {
        case '1':
            break;
        case '2':
            break;
        default:
            break;
    }

    return 0;
}

int main(int argc, char *argv[], char *envp[])
{
    int opt;
    /*
     * 单个字符a          表示选项a没有参数            格式：-a即可，不加参数
     * 单字符加冒号b:     表示选项b有且必须加参数      格式：-b 100或-b100,但-b=100错
     * 单字符加2冒号c::   表示选项c可以有，也可以无    格式：-c200，其它格式错误
     * char *string = "a::b:c:d";
     */
    char *cmd_str = "bt:";
    /*
     * b  : base opt
     * t: : test case ex: -t 1
     */

    while ((opt = getopt(argc, argv, cmd_str))!= -1)
    {
        printf("opt = %c\t\t", opt);
        printf("optarg = %s\t\t",optarg);
        printf("optind = %d\t\t",optind);
        printf("argv[optind] = %s\n",argv[optind]);

        switch(opt){
            case 'b':
                printf("======> base test <======\n");
                test_base();
                break;
            case 't':
                printf("======> test case %s <======\n", optarg);
                test_cases(optarg);
                break;
            default:
                break;
        }
    }

    return 0;
}
