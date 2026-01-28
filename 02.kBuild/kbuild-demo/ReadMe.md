这个demo主要演示了，以下文件之间的关系和逻辑
Kbuild.include
Makefile.build
Makefile
Kbuild

看 linux 的源码，Makefile.lib 貌似不咋使用了，因此这里不做太多探究

demo说明，主要关注Makefile.build :
1. obj-y obj-m 这些变量定义在 Makefile.build中
2. 进入一个文件夹之后Makefile.build 会 include Kbuild.include，其中定义了变量：
   kbuild-file，这指向当前目录下的 Kbuild/Makefile，Kbuild优先
3. obj-y/m 通过 include $(kbuild-file) 获得初始值
4. 对 obj-y/m 等进行处理，生成各种需要的变量
5. 对需要编译的文件，编译得到 build.a 或者 .o 等，这里没有演示
5. 对从 obj-y/m 中收集到的目录进行递归，走到下一级目录，然后循环往复，直到递归
   完成所有目录的编译

另外 Makefile.build 中会
-include $(objtree)/include/config/auto.conf
这也是make menuconfig 得到的镜像文件，它在这里使用
