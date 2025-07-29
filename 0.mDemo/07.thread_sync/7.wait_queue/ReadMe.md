学习驱动开发的所有demo示例
1.直接执行make指令可以生成kdemo.ko文件
2.执行 make init 可以加载模块
3.执行 make test 可以进行测试
4.执行 make exit 可以卸载模块



不同的平台，可能需要处理如下补丁的问题

diff --git a/0.mDemo/1.base/kDemo.c b/0.mDemo/1.base/kDemo.c
index a9ffb76..505c0c0 100644
--- a/0.mDemo/1.base/kDemo.c
+++ b/0.mDemo/1.base/kDemo.c
@@ -155,7 +155,7 @@ static struct class *m_chrdev_class = NULL;
 // array of m_chr_device_data for
 static struct m_chr_device_data m_chrdev_data[MAX_DEV];
 
-static int m_chrdev_uevent(struct device *dev, struct kobj_uevent_env *env)
+static int m_chrdev_uevent(const struct device *dev, struct kobj_uevent_env *env)
 {
     add_uevent_var(env, "DEVMODE=%#o", 0666);
     return 0;
@@ -258,7 +258,7 @@ static int __init m_chr_init(void)
     // create sysfs class
     // 创建设备节点的时候也用到了class，因此在/sys/class/m_chrdev_cls下可以看到
     // 链接到设备的软链接
-    m_chrdev_class = class_create(THIS_MODULE, "m_chrdev_cls");
+    m_chrdev_class = class_create("m_chrdev_cls");
     m_chrdev_class->dev_uevent = m_chrdev_uevent;
 
     // Create necessary number of the devices



wait queue 等待队列

等待队列是Linux内核提供的一种同步机制，它允许进程在某个条件不满足时挂起（睡眠），
直到条件满足或者被信号唤醒。

等待队列机制主要包括以下函数和宏：
* wait_event_interruptible:  
  这个宏允许进程在等待时被信号中断。如果进程在等待过程中接收到信号（例如，用户
  按下Ctrl+C），它会从等待状态中被唤醒，并且宏返回一个错误码（通常是 -ERESTARTSYS）。
  使用 wait_event_interruptible 的进程在等待时会释放CPU，并且可以被其他进程抢占。
* wait_event:  
  这个宏不允许进程在等待时被信号中断。即使进程接收到信号，它也会继续等待，直到条件
  满足或者被其他方式唤醒。
  使用 wait_event 的进程在等待时会释放CPU，但不会被信号中断。
* wake_up: 唤醒等待队列中的所有进程。
* wake_up_interruptible: 唤醒等待队列中所有可中断等待的进程。

等待队列通常用于设备驱动程序中，例如，当设备驱动需要等待硬件事件（如中断）或者资源
就绪时，就会使用等待队列。进程可以放入等待队列中，然后在某个条件得到满足时被唤醒。

等待队列机制是一种有效的同步手段，特别是在需要处理硬件事件或者需要响应信号的情况下。
它允许进程在等待时释放CPU，从而提高系统的整体性能。



## wait 函数对比

以下是包含 `wait_event_interruptible`、`wait_event_timeout` 和 `wait_event_interruptible_timeout` 三个关键函数的对比表格

---

### Linux 内核等待队列函数对比

| 特性                     | `wait_event_interruptible`         | `wait_event_timeout`               | `wait_event_interruptible_timeout`      |
|--------------------------|------------------------------------|------------------------------------|-----------------------------------------|
| **睡眠状态**             | `TASK_INTERRUPTIBLE`（可中断）     | `TASK_UNINTERRUPTIBLE`（不可中断） | `TASK_INTERRUPTIBLE`（可中断）          |
| **唤醒条件**             | 1. `condition` 为真<br>2. 信号中断 | 1. `condition` 为真<br>2. 超时到期 | 1. `condition` 为真<br>2. 信号中断<br>3. 超时到期 |
| **返回值**               | - `0`：条件满足<br>- `-ERESTARTSYS`：被信号中断 | - `>0`：剩余时间（条件满足）<br>- `0`：超时 | - `>0`：剩余时间（条件满足）<br>- `0`：超时<br>- `-ERESTARTSYS`：被信号中断 |
| **超时支持**             | ❌ 不支持                          | ✅ 支持（固定超时）                | ✅ 支持（固定超时 + 可中断）            |
| **典型用途**             | 阻塞等待（如驱动读取数据）         | 硬件超时检测、任务调度             | 需超时且需响应信号的场景（如用户态交互） |
| **参数示例**             | `wait_event_interruptible(wq, flag)` | `wait_event_timeout(wq, flag, timeout)` | `wait_event_interruptible_timeout(wq, flag, timeout)` |

---

### 关键说明：
1. **超时参数**：
   - `timeout` 以 `jiffies` 为单位，可用 `msecs_to_jiffies(ms)` 转换毫秒（例如 `msecs_to_jiffies(100)` 表示 100 毫秒）。
2. **信号中断**：
   - `_interruptible` 后缀的函数在收到信号（如 `SIGKILL`）时会立即返回 `-ERESTARTSYS`，需在调用处处理错误（例如重启操作或返回用户态）。
3. **竞态条件**：
   - 在调用这些宏前，通常需要结合锁（如 `spin_lock`）保护 `condition` 的修改和检查，避免并发问题。

---

### 如何选择？
- **需要无限等待 + 响应信号** → `wait_event_interruptible`
- **需要超时 + 不关心信号** → `wait_event_timeout`
- **需要超时 + 响应信号** → `wait_event_interruptible_timeout`

如果有其他具体场景的疑问，可以进一步讨论！
