# CPP_ThreadAndMemory

## CPP_Thread

路径：

```
CPP_ThreadAndMemory\CPP_Thread\
```

源码文件：

| 文件 | 内容 |
|---|---|
| `01.cpp` | 线程 与 mutex |
| `02.cpp` | 通用互斥锁管理（RAII） |
| `03.cpp` | `call_once` 仅调用一次，单例 Log |
| `04.cpp` | 条件变量，同步原语，允许多个线程相互通信 |
| `05.cpp` | 生产者消费者问题（Producer-consumer problem，也称有限缓冲问题） |
| `06.cpp` | 生产者消费者问题 类实现 |
| `07.cpp` | Promise 与 Future |
| `08.cpp` | packaged_task 与 async |
| `09.cpp` | 固定任务签名的专用线程池 |
| `10.cpp` | 泛型线程池 |
| `11.cpp` | atomic 原子操作 |
| `12.cpp` | 无锁链栈、无锁链队列、无锁循环队列 |
| `13.cpp` | 任务窃取线程池 |


---

## CPP_Memory

路径：

```

CPP_ThreadAndMemory\CPP_Memory\

```

源码文件：

| 文件 | 内容 |
|---|---|
| `01.cpp` | 析构函数（Destructor）与 构造函数（Constructor） |
| `02.cpp` | new / delete 表达式 与 operator new / delete 与 placement new |
| `03.cpp` | 默认分配器 `std::allocator` |
| `04.cpp` | 自定义 allocator |


---

## 工具脚本

| 文件 | 功能 |
|---|---|
| `merge_comments.py` | 合并全部 `.cpp` 文件中的 `/**/` 注释段 |