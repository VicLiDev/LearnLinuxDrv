**Linux 内核的 kfifo**，是一个**环形队列（FIFO）缓冲区**的实现，适合在内核中高效地
进行数据缓存和生产者-消费者通信。

---

## **1. kfifo 的基础**

* **kfifo** 是一个环形缓冲区，FIFO 的大小一般是 **2 的幂**，方便用位运算处理环绕。
* **线程安全**：如果一个线程只读，另一个线程只写，不需要加锁。
  如果多线程同时读或写，需要外部加锁。
* **存储单位**：可以存储字节流，也可以存储固定大小的元素（比如 `struct mpp_elog_node *` 指针）。

初始化有两种方式：
```c
DECLARE_KFIFO(my_fifo, int, 128);   // 静态分配，存 128 个 int
kfifo_alloc(&my_fifo, size, GFP_KERNEL);  // 动态分配
```

---

## **2. 常用 API 详解**

### **(1) kfifo_in**

```c
unsigned int kfifo_in(struct kfifo *fifo, const void *from, unsigned int n);
```
* **作用**：从 `from` 缓冲区中拷贝 `n` 个元素（或者字节，取决于 FIFO 类型）到 FIFO 尾部。
* **返回值**：实际写入的数量（可能小于 n，因为 FIFO 可能已满）。
* **使用场景**：批量写入，比如一次写一段日志、数据包。
* **工作逻辑**：
  * 检查剩余容量 → 计算写入位置 → 可能分两段写（尾部剩余部分 + 从头开始的部分）。
* **传输数据**：任何可按内存块拷贝的数据（结构体、指针、数组等）。

---

### **(2) kfifo_out**

```c
unsigned int kfifo_out(struct kfifo *fifo, void *to, unsigned int n);
```
* **作用**：从 FIFO 头部读出 `n` 个元素到 `to` 缓冲区，并移除这些数据。
* **返回值**：实际读出的数量（可能小于 n，因为 FIFO 数据不足）。
* **使用场景**：批量消费数据。
* **工作逻辑**：和 `kfifo_in` 类似，但读出后更新读指针。
* **传输数据**：和写入时一致。

---

### **(3) kfifo_put**

```c
int kfifo_put(struct kfifo *fifo, typeof(*fifo->type) val);
```
* **作用**：向 FIFO 写入**一个**元素（不是一段数据）。
* **返回值**：1 表示成功，0 表示失败（满了）。
* **使用场景**：适合放单个数据项（如一个指针、一条消息）。
* **工作逻辑**：检查空间 → 写入到 `in` 指针位置 → `in++`（环绕）。
* **传输数据**：固定大小的元素（如 `int`、指针）。

---

### **(4) kfifo_peek**

```c
int kfifo_peek(struct kfifo *fifo, typeof(*fifo->type) *val);
```
* **作用**：读取 FIFO **头部的一个元素**，但不移除它。
* **返回值**：1 表示成功，0 表示失败（FIFO 为空）。
* **使用场景**：查看下一个将被消费的元素，不破坏队列。
* **工作逻辑**：直接读取 `out` 指针位置的元素，但不更新 `out`。
* **传输数据**：固定大小的元素。

---

### **(5) kfifo_get**

```c
int kfifo_get(struct kfifo *fifo, typeof(*fifo->type) *val);
```
* **作用**：读取 FIFO **头部的一个元素**，并将它移除。
* **返回值**：1 表示成功，0 表示失败（FIFO 为空）。
* **使用场景**：消费一个消息。
* **工作逻辑**：读取 `out` 位置 → `out++`（环绕）。
* **传输数据**：固定大小的元素。

---

### **(6) kfifo_to_user**

```c
unsigned int kfifo_to_user(struct kfifo *fifo,
                           void __user *to,
                           unsigned int n,
                           unsigned int *copied);
```
* **作用**：将 FIFO 中的数据拷贝到用户空间 `to`。
* **返回值**：0 表示成功，负数表示错误。
* **使用场景**：在 `read()` 文件操作中，把内核缓冲区数据直接送给用户空间。
* **工作逻辑**：和 `kfifo_out` 类似，但多了用户空间访问检查（`copy_to_user`）。
* **传输数据**：任意字节流。

---

### **(7) kfifo_from_user**

```c
unsigned int kfifo_from_user(struct kfifo *fifo,
                             const void __user *from,
                             unsigned int n,
                             unsigned int *copied);
```
* **作用**：从用户空间 `from` 拷贝数据到 FIFO 尾部。
* **返回值**：0 表示成功，负数表示错误。
* **使用场景**：在 `write()` 文件操作中，把用户写入的数据保存到内核 FIFO。
* **工作逻辑**：和 `kfifo_in` 类似，但多了 `copy_from_user`。
* **传输数据**：任意字节流。

---

## **3. 小总结表格**

| API               | 方向 | 操作单位 | 是否批量 | 是否移除数据 | 典型用途       |
| ----------------- | ---- | -------- | -------- | ------------ | -------------- |
| `kfifo_in`        | 写   | 任意     | 批量     | N/A          | 写入一段数据   |
| `kfifo_out`       | 读   | 任意     | 批量     | 是           | 读取一段数据   |
| `kfifo_put`       | 写   | 元素     | 单个     | N/A          | 写入一个元素   |
| `kfifo_get`       | 读   | 元素     | 单个     | 是           | 读取一个元素   |
| `kfifo_peek`      | 读   | 元素     | 单个     | 否           | 查看下一个元素 |
| `kfifo_to_user`   | 读   | 任意     | 批量     | 是           | 内核→用户      |
| `kfifo_from_user` | 写   | 任意     | 批量     | N/A          | 用户→内核      |

---

## **4. 能传输什么数据**

* **字节流模式**（kfifo_alloc时指定字节大小）：可以传任意二进制数据（字符串、结构体、原始缓冲）。
* **元素模式**（DECLARE_KFIFO指定类型）：适合传固定大小的类型（int、指针、结构体）。
* 不能直接存储**变长数据**，但可以存**指针**，指向真实数据块。

实际上，**kfifo 不关心数据类型**，**kfifo** 在 API 级别其实并不关心你放的是什么
“数据类型”，它只认**字节流**。

`kfifo` 底层就是一个**循环字节数组**（ring buffer），所有 API 都是按**字节**来存取数据。
它完全不知道你放进去的是：
* `int`
* `struct m_cls *`
* 原始字节数据

所以：
* 如果用 `kfifo_put()` / `kfifo_get()`，它会按元素拷贝（需要你自己指定大小）。
* 如果用 `kfifo_in()` / `kfifo_out()`，它就是单纯 memcpy。

例子：存指针
```c
struct m_cls *node = ...;
kfifo_put(&m_fifo, node); // FIFO 元素类型是 struct m_cls *
```

例子：存原始数据
```c
char buf[128];
kfifo_in(&m_fifo, buf, 128);
```

---

**重点提示**
* 如果 FIFO 里存放**指针**，**不要**用 `kfifo_in()`/`kfifo_out()`（它是字节拷贝），
  用 `kfifo_put()` / `kfifo_get()` 配合 `DECLARE_KFIFO_PTR()` 来管理。
* 如果存放**原始字节数据**（包、消息、流），就用 `kfifo_in()` / `kfifo_out()`。
* 如果用 `kfifo_alloc()`，退出时一定记得 `kfifo_free()`，否则内存泄漏。



## `kfifo_in/out` vs `kfifo_put_get`

### **`kfifo_in` / `kfifo_out`**

* **作用**
  * `kfifo_in()`：往 FIFO 里写入一段**原始数据（字节流）**。
  * `kfifo_out()`：从 FIFO 里读出一段**原始数据（字节流）**。
* **使用方式**
  ```c
  char buf[16];
  kfifo_in(&fifo, buf, sizeof(buf));   // 把 buf 的内容复制进 FIFO
  kfifo_out(&fifo, buf, sizeof(buf));  // 从 FIFO 拷贝数据到 buf
  ```
* **特点**
  * 需要你**提供缓冲区地址**和**长度**。
  * FIFO 内部存的是一段连续的内存拷贝（字节数组）。
  * 适合存放原始数据块，比如 `char[]`、二进制帧等。
  * 不关心你存的是什么类型，它就是 memcpy 进去，memcpy 出来。

---

### **`kfifo_put` / `kfifo_get`**

* **作用**
  * `kfifo_put()`：往 FIFO 里放入一个**完整的元素**（不是字节流）。
  * `kfifo_get()`：从 FIFO 里取出一个**完整的元素**。
* **使用方式（类型安全版 FIFO）**
  ```c
  struct foo *ptr = ...;
  kfifo_put(&fifo, ptr);    // 把 ptr 作为一个元素放入 FIFO
  kfifo_get(&fifo, &ptr);   // 从 FIFO 取出一个元素
  ```
* **特点**
  * 需要 `DECLARE_KFIFO(name, type, size)` 或 `DECLARE_KFIFO_PTR(name, type)` 定义时指定类型。
  * 编译器会检查类型匹配（类型安全）。
  * FIFO 里的每个元素大小就是 `sizeof(type)`。
  * 存取时不需要自己管 memcpy，大部分情况下是按值存储，但**如果 type 是指针，那存的就是指针本身**。

---

### **为什么放指针不用 `kfifo_in` / `kfifo_out`**

* 如果你用 `kfifo_in/out` 存指针：
  * 你需要 `kfifo_in(&fifo, &ptr, sizeof(ptr))`，本质上是 memcpy 指针变量的地址。
  * 容易写错长度，比如写成 `sizeof(*ptr)` 就会把整个结构体拷到 FIFO（浪费空间）。
  * 读的时候还得 `kfifo_out(&fifo, &ptr, sizeof(ptr))`，很啰嗦。
* 如果你用 `kfifo_put/get`：
  * 定义 FIFO 时直接 `DECLARE_KFIFO_PTR(node_fifo, struct mpp_elog_node *);`
  * 存的时候 `kfifo_put(&node_fifo, ptr)`，取的时候 `kfifo_get(&node_fifo, &ptr)`，类型安全，简洁不容易错。


### **结论**：
* 存放**任意长度的原始数据** → 用 `kfifo_in/out`。
* 存放**固定大小的元素**（特别是指针、结构体等） → 用 `kfifo_put/get`。
