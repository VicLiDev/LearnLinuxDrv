# `task_struct`、`kthread_worker`、`work_struct` 和 `delayed_work`

---

## 一览：四者的对比

在 Linux 内核中，有多种机制用于异步任务处理和线程控制。以下是常用的几种结构及其差异：

1. **`task_struct`**
   * **类型**：数据结构
   * **机制类别**：线程/进程调度系统
   * **用途**：表示内核中每一个线程或进程的核心描述符。包含调度状态、内存信息、信号、
     CPU 绑定等。
   * **是否控制线程生命周期**：✅ 支持完全控制（如创建、停止、唤醒等）
   * **是否支持延迟执行**：❌ 不支持延迟调度逻辑（需配合定时器）
   * **运行上下文**：内核线程或进程上下文
   * **典型用法**：`kthread_create()` 创建内核线程、访问当前线程用 `current`，调试或
     调度操作等

2. **`kthread_worker`**
   * **类型**：数据结构
   * **机制类别**：自定义线程工作队列
   * **用途**：调度多个 `kthread_work` 或 `kthread_delayed_work`，由一个内核线程运行，
     适用于驱动等对线程控制要求高的场景。
   * **是否控制线程生命周期**：✅ 可以控制（由你创建和管理线程）
   * **是否支持延迟执行**：✅ 支持（使用 `kthread_delayed_work`）
   * **运行上下文**：内核线程上下文
   * **典型用法**：初始化一个 `kthread_worker`，用 `kthread_run()` 创建线程执行
     `kthread_worker_fn()`，适用于长期运行的 worker 线程场景

3. **`work_struct`**
   * **类型**：数据结构
   * **机制类别**：工作队列（workqueue）
   * **用途**：提交一个异步函数到内核的系统工作线程池中执行，适合短时、轻量的任务。
   * **是否控制线程生命周期**：❌ 不支持（线程由内核统一管理）
   * **是否支持延迟执行**：❌ 不支持（不能直接延迟，需要配合 `delayed_work`）
   * **运行上下文**：软中断上下文或工作线程上下文（非中断）
   * **典型用法**：使用 `INIT_WORK()` 初始化，调用 `schedule_work()` 提交任务

4. **`delayed_work`**
   * **类型**：结构体宏（包含 `work_struct` + `timer_list`）
   * **机制类别**：延迟工作队列
   * **用途**：在延迟一段时间后，将任务提交给系统工作队列执行。
   * **是否控制线程生命周期**：❌ 不支持（使用系统线程池）
   * **是否支持延迟执行**：✅ 支持（通过定时器机制）
   * **运行上下文**：同 `work_struct`，运行在工作队列线程中
   * **典型用法**：使用 `INIT_DELAYED_WORK()` 初始化，调用 `schedule_delayed_work()`
     设置延迟并提交执行

tops:
* `task_struct` 是线程的基础，贯穿所有机制。
* `work_struct` 和 `delayed_work` 是 **“面向任务”的队列执行机制**。
* `kthread_worker` 是 **“面向线程”的执行机制** ，更强大但也需要自己管理线程生命周期。

## kthread_worker/work_struct 详细对比

kthread_worker 和 work_struct 没有包含关系，属于完全**不同的机制。
它们分别服务于两种不同的工作队列模型：
| 项目             | `kthread_worker`                          | `work_struct`                        |
| ---------------- | ----------------------------------------- | ------------------------------------ |
| 工作模型         | 自建线程 → 执行多个任务                   | 系统内核线程池 → 执行一个个任务      |
| 任意线程执行？   | 只在绑定的 `kthread_worker` 线程执行      | 任意由内核调度的 worker 线程         |
| 控制力           | 高度控制线程行为                          | 无法控制具体执行线程                 |
| 是否有包含关系？ | 无任何结构体层级或嵌套关系                | 与 `kthread_worker` 完全无关         |
| 线程生命周期     | 需要负责创建、停止 `kthread_worker` 线程  | 内核管理线程池生命周期               |
| 接口调用         | `kthread_queue_work()` 等                 | `schedule_work()`、`flush_work()` 等 |


类比理解（打工模型）：
| 概念             | 类比角色                     | 特点                       |
| ---------------- | ---------------------------- | -------------------------- |
| `work_struct`    | 给系统临时找的打工人         | 人是谁不重要，只要干活就行 |
| `kthread_worker` | 自己雇的员工团队             | 必须在你安排的线程里工作   |
| `kthread_work`   | 分给 `kthread_worker` 的任务 | 它不能投给 `work_struct`   |



### 工作逻辑对比

| 特性              | `work_struct`（通用工作队列）                                            | `kthread_work`（内核线程工作队列）                                     |
| ----------------- | ------------------------------------------------------------------------ | ---------------------------------------------------------------------- |
| **核心思想**      | 把任务放进内核**全局/自定义 workqueue**，由 worker 线程池执行            | 把任务放进你自己创建的 **kthread_worker** 队列，由你自己的内核线程执行 |
| **调度主体**      | 系统维护的 **kworker** 线程池（每 CPU 通常一个或多个线程）               | 你创建的 **kthread_worker**（通常就是一个内核线程）                    |
| **初始化**        | `INIT_WORK(&work, handler)`                                              | `kthread_init_work(&work, handler)`                                    |
| **提交任务**      | `schedule_work(&work)`（默认队列）或 `queue_work(wq, &work)`（指定队列） | `kthread_queue_work(worker, &work)`                                    |
| **任务存储位置**  | workqueue 的链表                                                         | kthread_worker 的链表                                                  |
| **执行线程数量**  | 默认是多线程（可能跨 CPU 并行）                                          | 一般单线程（串行执行）                                                 |
| **线程控制权**    | 完全由内核调度                                                           | 由你完全控制（可暂停、停止、绑定 CPU）                                 |
| **CPU 亲和性**    | 系统自动选择（可能跨 CPU）                                               | 你可以绑死到指定 CPU                                                   |
| **使用场景**      | 短小异步任务，不需要严格顺序                                             | 长耗时任务、需要串行顺序、需要精确线程控制                             |

---

### 并行性对比

#### `work_struct`

* **默认 workqueue** → 多 worker 线程 → 多 CPU 上可并行执行不同任务
* 同一个 `struct work_struct` 任务：
  * **不能并行**（因为内部有 `pending` 标志）
  * 如果任务正在执行，重复提交会被忽略
* 不同的 `struct work_struct` → **可能同时运行在不同 CPU**

---

#### `kthread_work`

* 同一个 `kthread_worker` **一次只执行一个任务** → **绝对串行**
* 即使提交多个任务，也会一个接一个执行
* 如果要并行，需要自己创建多个 `kthread_worker`（多个线程）

---

### 执行流程图

```
work_struct（多线程模型）:
   schedule_work() → 加入 system_wq
                     ↓
            多个 kworker 线程争抢任务
             CPU0 执行任务A   CPU1 执行任务B   CPU2 执行任务C
            （并行执行）

kthread_work（单线程模型）:
   kthread_queue_work() → 加入 kthread_worker 队列
                            ↓
             唯一的 worker 线程顺序取任务
              执行任务A → 执行任务B → 执行任务C
            （严格串行）
```

---

### 总结

* **想省事** → 用 `work_struct`，由系统线程池自动并行调度
* **想保证顺序** / **长耗时任务** / **精确控制线程生命周期** → 用 `kthread_work`
* **同一个任务结构体**（无论哪种）在执行中不会被并行运行，但不同任务的并行情况取决于线程模型


---

## 1. `struct task_struct`

> 🔧 **内核中每个线程（包括内核线程和用户进程）的描述符**

* 包含调度信息、内存映射、状态等。
* 内核线程使用它通过 `kthread_create()` / `kthread_run()` 创建。
* 用户空间进程也使用它，是 `current` 宏返回的内容。

```c
struct task_struct *t = current;
pr_info("Current task is: %s (pid: %d)", t->comm, t->pid);
```

* 使用场景：调试线程、调度控制、阻塞唤醒、调试等。

---

## 2. `struct kthread_worker`

> 🧵 **一个专门处理 `kthread_work` 的工作者线程调度器**

* 用于调度多个 `kthread_work` 和 `kthread_delayed_work`。
* 自定义线程控制，适合驱动或模块内的工作池需求。
* 必须结合一个 `task_struct` 来运行，如：

```c
kthread_init_worker(&worker);
task_struct *thread = kthread_run(kthread_worker_fn, &worker, "my_worker");
```

* 可以手动调度、延迟、取消、同步等待任务完成。

适合对线程控制有明确需求的场景，例如：
✔ 实时驱动
✔ 大量异步任务排队执行
✔ 可取消任务模型

---

## 3. `struct work_struct`

> ⚙️ **一个函数包装器，用于提交异步任务到系统工作队列**

* 配合 `INIT_WORK()` 和 `schedule_work()` 使用。
* 适合轻量级、非实时的异步处理。
* 不绑定特定线程，由系统全局线程池自动调度。

示例：

```c
INIT_WORK(&my_work, work_func);
schedule_work(&my_work);
```

* 通常运行在 `kworker/*` 系统线程中。
* 一般不保证实时性、不保证顺序，不适合处理时序敏感或互斥强的场景。

---

## 4. `struct delayed_work`

> ⏳ **带定时器的延迟版本 work_struct**

* 实质是一个结构体组合：

```c
struct delayed_work {
    struct work_struct work;
    struct timer_list timer;
};
```

* 用于延迟执行任务，适合定时重试、timeout handler 等。

```c
INIT_DELAYED_WORK(&dw, work_func);
schedule_delayed_work(&dw, msecs_to_jiffies(1000)); // 1秒后执行
```

* 执行时机靠定时器调度，然后自动提交到系统工作队列。

---

## 使用建议和典型组合

| 目标                                   | 建议用法                          | 原因                         |
| -------------------------------------- | --------------------------------- | ---------------------------- |
| 需要创建你自己的线程，并调度多个工作项 | `kthread_worker` + `kthread_work` | 灵活、高性能、可取消、可并行 |
| 需要一次性的异步执行函数               | `work_struct`                     | 简单轻量，不需管理线程       |
| 需要延迟一段时间后异步执行任务         | `delayed_work`                    | 使用内核定时器调度任务       |
| 控制线程行为（睡眠、阻塞、信号）       | `task_struct`                     | 线程控制核心结构             |


## kthread_worker 详解

---

### 什么是 `kthread_worker` 机制？

`kthread_worker` 是 Linux 内核提供的一个**基于内核线程的异步工作队列系统**。
它跟 `workqueue` 类似，但更轻量、私有、实时性更可控。

它内部通过：
* 一个内核线程（kthread）
* 一个工作队列（FIFO 队列）
* 不断从队列取出任务执行

来完成异步任务处理。

---

### 核心结构体和函数关系图

按**结构体** 和 **函数** 来理清楚谁负责做什么：
```
自定义工作函数（void my_work_fn(struct kthread_work *work)）
         ↑
   执行器：__kthread_run_work(worker, work)
         ↑
工作线程：kthread_worker_fn(worker)  <-- 注意这个！！
         ↑
创建函数：kthread_create_worker()
         ↑
启动线程：kthread_create() 内部调用的线程主体就是 kthread_worker_fn
```

---

### 主要结构体解析

#### 1. `struct kthread_worker`

```c
struct kthread_worker {
    spinlock_t lock;
    struct list_head work_list;     // 存放 kthread_work
    struct task_struct *task;      // 线程本体（kthread_create() 生成）
};
```

它就是一个整体容器，包含了：
* 一个“任务队列”
* 一个“线程”

---

#### 2. `struct kthread_work`

```c
struct kthread_work {
    struct list_head node;
    void (*func)(struct kthread_work *work);  // 你写的工作函数
    atomic_t flushing;
    ...
};
```

提交给 `kthread_worker` 的每一个任务就是一个 `kthread_work` 对象。

---

### 核心函数说明

#### 1. `kthread_create_worker()`
```c
struct kthread_worker *kthread_create_worker(unsigned int flags, const char *namefmt, ...);
```
* 创建并初始化一个 `kthread_worker`
* 内部调用 `kthread_create(kthread_worker_fn, worker, namefmt)` 启动线程
* 会设置好 `worker->task` 字段
* **核心点**：这个函数是 `kthread_worker_fn()` 唯一的合法使用入口

#### 2. `kthread_init_worker(struct kthread_worker *worker)`
* 初始化一个已有的 worker 结构体，但不启动线程
* 当希望想自己控制线程生命周期时使用

#### 3. `kthread_worker_fn(void *worker)`
* 线程主函数（不要自己用！）
* 不断从 `worker->work_list` 中拉取任务执行
* 会调用 `__kthread_run_work(worker, work)` 来执行每个工作项

#### 4. `kthread_init_work()` / `kthread_queue_work()`
```c
void kthread_init_work(struct kthread_work *work, void (*fn)(struct kthread_work *));
bool kthread_queue_work(struct kthread_worker *worker, struct kthread_work *work);
```
* 初始化一个 `work` 对象，指定函数
* 提交任务到指定 `worker` 的队列中，自动唤醒线程

#### 5. `kthread_flush_work(struct kthread_work *work)`
* 等待这个 work 被执行完
* 类似 flush_work()，用于同步

#### 6. `kthread_destroy_worker(struct kthread_worker *worker)`
* 停止线程并释放 worker 资源
* 释放资源

#### 7. `kthread_should_stop()`
* 用于线程主函数中判断是否要退出
* 自写线程 loop 时使用

---

### 结构图：各组件关系

#### kthread_create_worker 自动方式
```
┌───────────────────────────────┐
│      struct kthread_worker    │<─────────────────────────┐
│ ┌──────────────────────────┐  │                          │
│ │ spinlock_t lock          │  │                          │
│ │ struct list_head list    │  │                          │
│ │ struct task_struct *task │  │◄────────┐                │
│ └──────────────────────────┘  │         │                │
└────────────┬──────────────────┘         │                │
             │                            │                │
             ▼                            │                │
┌────────────────────────────┐      ┌─────▼────────────────────┐
│     struct kthread_work    │─────►│   kthread_queue_work()   │
│ ┌───────────────────────┐  │      └───────────┬──────────────┘
│ │ struct list_head node │  │                  │
│ │ void (*func)(...)     │  │                  ▼
│ └───────────────────────┘  │          ┌───────────────────────┐
└────────────────────────────┘          │ kthread_worker_fn()   │
                                        │ (线程主循环函数)      │
                                        │ while (!stop) {       │
                                        │    get work from list │
                                        │    call work->func()  │
                                        │ }                     │
                                        └───────────────────────┘
```

#### kthread_init_worker 手动方式
```
手动方式整体结构：

┌────────────────────────────────────────────────┐
│               struct kthread_worker            │
│ ┌────────────────────────────────────────────┐ │
│ │ spinlock_t lock                            │ │
│ │ struct list_head work_list                 │ │
│ │ struct task_struct *task (未自动设置)      │ │◄── 你自己必须启动线程，并执行 worker_fn
│ └────────────────────────────────────────────┘ │
└────────────────────────────────────────────────┘
                        ▲
                        │
                        │   使用：
                        │   kthread_init_worker(&worker);
                        │
        ┌───────────────┴────────────────┐
        ▼                                ▼
┌──────────────────────────────┐   ┌────────────────────────────────────────┐
│ struct kthread_work (任务项) │   │ 线程：由你调用 kthread_run() 创建的    │
│ ┌──────────────────────────┐ │   │ 执行函数调用 kthread_worker_fn()       │
│ │ void (*func)(...)        │ │   │ 它会从 worker->work_list 中拉任务      │
└──────────────────────────────┘   └────────────────────────────────────────┘
```

---

### 流程图（时序）

#### kthread_create_worker 自动方式
```
自定义模块初始化函数
      │
      ▼
┌────────────────────────────────────────────┐
│ worker = kthread_create_worker(...);       │◄──── 内核创建线程并设置 worker->task
└────────────────────────────────────────────┘
      │
      ▼
┌────────────────────────────────────────────┐
│ kthread_init_work(&work, my_work_fn);      │
└────────────────────────────────────────────┘
      │
      ▼
┌──────────────────────────────────────────────┐
│ kthread_queue_work(worker, &work);           │◄──── 把任务放入 worker->work_list
└──────────────────────────────────────────────┘
      │
      ▼
┌────────────────────────────────────────────────┐
│ 内核线程开始运行：kthread_worker_fn(worker)    │
│  --> while(1):                                 │
│       - 从 work_list 取出任务                  │
│       - 调用 work->func(&work);                │
│       - 继续下一个                             │
└────────────────────────────────────────────────┘
```

#### kthread_init_worker 手动方式
```
自定义模块初始化函数
      │
      ▼
┌─────────────────────────────────────┐
│ struct kthread_worker my_worker;    │
│ kthread_init_worker(&my_worker);    │ ← 初始化但不启动线程
└─────────────────────────────────────┘
      │
      ▼
┌───────────────────────────────────────┐
│ task = kthread_run(my_thread_fn, ...);│ ← 你自己创建线程
└───────────────────────────────────────┘
      │
      ▼
┌──────────────────────────────────────────┐
│ int my_thread_fn(void *arg)              │
│ { return kthread_worker_fn(&my_worker); }│ ← worker主函数
└──────────────────────────────────────────┘
      │
      ▼
┌────────────────────────────────────────────┐
│ kthread_worker_fn()                        │
│ - while (!kthread_should_stop()) {         │
│     work = get from work_list              │
│     call work->func()                      │
│   }                                        │
└────────────────────────────────────────────┘
      ▲
      │
┌──────────────────────────────────────────────┐
│ kthread_init_work(&work, my_work_fn);        │
│ kthread_queue_work(&my_worker, &work);       │ ← 提交工作任务
└──────────────────────────────────────────────┘
```

---

#### 自动方式和手动方式对比

| 比较项            | `kthread_create_worker()` 自动方式 | `kthread_init_worker()` 手动方式 |
| ----------------- | ---------------------------------- | -------------------------------- |
| worker 分配       | 自动分配并初始化                   | 自己分配栈上或堆上               |
| 线程创建          | 内部创建并绑定                     | 自己用 `kthread_run()` 创建      |
| 主函数            | 内部绑定 `kthread_worker_fn()`     | 手动写线程函数调用它             |
| worker->task 设置 | 自动设置为线程                     | 必须手动设置或忽略它             |
| 灵活性            | 简洁封装                           | 更灵活可控（优先级、绑定 CPU 等）|
| 推荐程度          | ✅ 高                              | ⚠️ 中（有需求时）                 |

---

### 为什么 **不能** 手动调用 `kthread_worker_fn`？

因为它是**线程主循环函数**，不是你业务逻辑的一部分。

#### 举例说明：

这样写：
```c
task = kthread_run(kthread_worker_fn, worker, "my_kthread");
```

问题来了：
* `worker->task` 是空的
* `kthread_worker_fn` 进来会 `WARN_ON(worker->task != current)`
* 崩了 ❌

而内核原始设计是这样的：
```c
worker = kthread_create_worker();
  -> 调用 kthread_create(kthread_worker_fn, worker, ...)
     -> 设置 worker->task = 创建的线程
```

绕过 `kthread_create_worker`，就破坏了这个机制。

---

#### 最小合法用法示例

```c
static struct kthread_worker *worker;
static struct kthread_work my_work;

void my_work_fn(struct kthread_work *work)
{
    pr_info("Running my work function!\n");
}

static int __init mymod_init(void)
{
    worker = kthread_create_worker(0, "my_worker_thread");
    if (IS_ERR(worker)) {
        pr_err("Failed to create worker thread\n");
        return PTR_ERR(worker);
    }

    kthread_init_work(&my_work, my_work_fn);
    kthread_queue_work(worker, &my_work);
    return 0;
}

static void __exit mymod_exit(void)
{
    kthread_destroy_worker(worker);
}
```

只需管：
* 定义好 `work` 和 `my_work_fn`
* 用 `kthread_create_worker()` 创建线程
* 提交 `kthread_queue_work()` 即可


---

#### 错误做法 vs 正确做法对比

| 错误代码                              | 问题                                                     |
| ------------------------------------- | -------------------------------------------------------- |
| `kthread_run(kthread_worker_fn, ...)` | **错误**使用了内核线程的主循环函数，绕过了 worker 初始化 |
| `worker->task == NULL`                | 会触发 `WARN_ON()`，线程未绑定到 worker 上               |

| 正确方式                               | 说明                               |
| -------------------------------------- | ---------------------------------- |
| `worker = kthread_create_worker(...);` | 自动创建线程，绑定 `worker->task`  |
| 提交工作项                             | 用 `kthread_queue_work()` 正常调度 |

---

#### 用 `kthread_run()` 创建线程，该怎么做？

就**别用 `kthread_worker` 系统**，直接自己写个线程函数：

```c
int my_thread_fn(void *arg)
{
    while (!kthread_should_stop()) {
        // do something
        msleep(1000);  // 模拟周期任务
    }
    return 0;
}
```

## 资源释放

---

### 1. `task_struct` 的资源释放

* **作用**：Linux 内核中表示进程/线程的核心结构。
* **生命周期管理**：
  * 由内核自动管理，绑定于进程生命周期。
  * 进程退出时，内核会自动释放 `task_struct` 及其相关资源（如内存映射、文件描述符等）。
  * 仅当所有引用计数为0时，才真正释放。
* **是否需手动释放？**
  **不需要。** 由内核自动管理，开发者无需手动释放。

---

### 2. `kthread_worker` 的资源释放

* **作用**：用于管理内核线程池，串行执行内核工作队列的工作。
* **创建方式 & 生命周期**：
  * **动态创建**：通过 `kthread_worker_create()` 分配，内核线程动态生成。
  * **静态创建**：如 `static struct kthread_worker my_worker`，内存静态分配，无动态释放。
* **资源释放**：
  * **动态创建的 `kthread_worker`**
    * 必须调用 `kthread_worker_destroy()` 停止线程并释放资源。
    * 内部调用 `kthread_stop()` 停止线程后，释放内存。
  * **静态创建的 `kthread_worker`**
    * 不需调用销毁函数，静态内存由编译期分配，线程资源可通过调用 `kthread_worker_init()` 初始化。
    * 一般不做销毁，直接用即可。
* **是否需手动释放？**
  * 动态创建：**需要手动停止并销毁**
  * 静态创建：**不需要销毁**

---

### 3. `work_struct` 的资源释放

* **作用**：表示一个可延迟执行的工作任务。
* **生命周期 & 内存分配**：
  * **静态分配**（全局变量或栈变量）：内存自动管理，随模块卸载或函数结束释放。
  * **动态分配**（如 `kmalloc`）：内存需手动释放。
* **资源释放步骤**：
  * 保证工作已经完成或取消，调用 `cancel_work_sync()` 等待工作执行完毕。
  * **动态分配的内存**：需在工作完成后调用 `kfree()` 释放内存。
  * **静态分配**：无需额外释放。
* **是否需手动释放？**
  * 静态分配：**不需要释放**
  * 动态分配：**需要取消工作并释放内存**

---

### 关键总结表

| 结构体           | 创建方式   | 需手动释放？ | 手动释放操作                               |
| ---------------- | ---------- | ------------ | ------------------------------------------ |
| `task_struct`    | 由内核管理 | 否           | 内核自动释放                               |
| `kthread_worker` | 动态创建   | 是           | 调用 `kthread_worker_destroy()` 停止并释放 |
| `kthread_worker` | 静态创建   | 否           | 无需销毁，静态分配                         |
| `work_struct`    | 动态分配   | 是           | 调用 `cancel_work_sync()` + `kfree()`      |
| `work_struct`    | 静态分配   | 否           | 无需释放                                   |


## 关于 kthread_stop

---

### 1. `task_struct` (kthread)

* **背景：**
  `task_struct` 代表内核中的线程或进程，你用 `kthread_create()` 创建的内核线程，
  就是用 `task_struct` 表示的。
* **什么时候需要调用 `kthread_stop`？**
  只要你创建了内核线程（kthread），并且不再需要它时，都应该调用 `kthread_stop`
  来安全停止它。
  这是停止线程的标准接口，会告诉线程停止运行（让 `kthread_should_stop()` 返回真），
  并等待线程函数返回。
* **总结：**
  你负责启动线程也必须负责调用 `kthread_stop` 停止线程，避免线程永远运行造成资源泄漏。

---

### 2. `kthread_worker`

* **背景：**
  * `kthread_worker` 是一种封装了内核线程的工作池对象，通常由 `kthread_worker_create()`
    动态创建。
* **什么时候需要调用 `kthread_stop`？**
  * *不直接调用 `kthread_stop`*。调用的是 `kthread_worker_destroy()`，它内部会自动调用
    `kthread_stop()` 来停止对应的内核线程。
* **静态 vs 动态**
  * **动态创建的 `kthread_worker`**（调用了 `kthread_worker_create`）需要在不再使用时
    调用 `kthread_worker_destroy()`，间接调用 `kthread_stop` 来停止线程。
  * **静态定义的 `kthread_worker`** （例如 `static struct kthread_worker worker;`）
    一般不需要调用销毁函数，线程随模块退出自动释放。
* **总结：**
  * 不要直接调用 `kthread_stop`，调用 `kthread_worker_destroy()` 即可，动态创建的
    必须手动销毁，静态的可不用。

---

### 3. `work_struct`

* **背景：**
  * `work_struct` 是内核工作队列里的任务单元，不直接表示线程，而是一个调度单元，
    内核线程负责执行。
* **什么时候需要调用 `kthread_stop`？**
  * **不需要**。`work_struct` 本身没有独立线程，它是被内核的工作线程池执行的。
    只需管理好工作本身：
  * 用 `cancel_work_sync()` 确保工作不再运行（同步取消），
  * 对动态分配的 `work_struct` 负责释放内存。
* **总结：**
  * `work_struct` 不涉及 `kthread_stop`，它依赖内核工作线程池，用户管理的是工作本身
    的取消和资源释放。

---

### 总结：

| 结构体                   | 调 `kthread_stop` | 具体操作                             | 备注                                                 |
| ------------------------ | ----------------- | ------------------------------------ | ---------------------------------------------------- |
| `task_struct`（kthread） | 是                | 创建后停止时调用 `kthread_stop()`    | 自己创建的内核线程，必须停止避免泄漏                 |
| `kthread_worker`         | 否                | 销毁时调用 `kthread_worker_destroy()`| `kthread_worker_destroy` 内部调用了 `kthread_stop()` |
| `work_struct`            | 否                | 取消工作用 `cancel_work_sync()`      | 由内核工作线程池调度执行，无独立线程                 |


* **自己用 `kthread_create` 创建线程，必须调用 `kthread_stop` 停止线程。**
* **使用 `kthread_worker` 时，调用 `kthread_worker_destroy`，它内部负责调用 `kthread_stop`。**
* **管理 `work_struct` 时，不用管 `kthread_stop`，只需确保工作执行完成或取消。**

