/*************************************************************************
    > File Name: container_of.c
    > Author: LiHongjin
    > Mail: 872648180@qq.com
    > Created Time: Thu 23 May 2024 03:26:30 PM CST
 ************************************************************************/

#include <stdio.h>

/* 假设的结构体 */
typedef struct {
    int member1;
    int member2;
    /* 其他成员... */
} MyStruct;


/*
 * offsetof 宏也在 Linux 内核中有定义，在用户空间，stddef.h 中有类似的定义
 * kernel 中的定义如下
 */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/* container_of 宏的定义，跟kernel里的一样 */
#define container_of(ptr, type, member) ({          \
    const typeof(((type *)0)->member) *__mptr = (ptr);    \
    (type *)((char *)__mptr - offsetof(type, member)); \
})


int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    /* 创建一个 MyStruct 的实例并初始化其 member */
    MyStruct my_instance = { .member1 = 11, .member2 = 22 };

    /* 获取 member 的地址 */
    int *member2_ptr = &my_instance.member2;

    /* 使用 container_of 宏来获取指向 my_instance 的指针 */
    MyStruct *parent_ptr = container_of(member2_ptr, MyStruct, member2);

    /* 检查我们是否得到了正确的指针 */
    if (parent_ptr == &my_instance) {
        printf("container_of worked correctly! member1:%d member2:%d\n",
                parent_ptr->member1, parent_ptr->member2);
    } else {
        printf("container_of failed.\n");
    }

    return 0;
}
