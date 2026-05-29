# C++ 并发学习总结

---

## 1. 线程基础：`std::thread` / `std::jthread`

### 1.1 `std::thread` 的核心认识

- `std::thread` 表示一个执行线程，用于并发执行函数。
- 构造时传入“可调用对象 + 参数”。
- 参数默认按值传递。
- 如果线程函数参数需要引用，必须显式使用 `std::ref`。
- 被引用对象的生命周期必须覆盖线程使用期间。
- `std::thread` 支持移动，不支持拷贝。

### 1.2 常用成员函数

- `joinable()`：判断当前线程对象是否关联了实际线程。
- `get_id()`：获取线程 id。
- `join()`：等待线程结束。
- `detach()`：分离线程，线程独立运行，之后线程对象失去控制权。

### 1.3 生命周期陷阱

- 如果 `std::thread` 在析构时仍然 `joinable() == true`，程序会调用 `std::terminate()`。
- 所以线程对象在离开作用域前，必须 `join()` 或 `detach()`。

### 1.4 `std::jthread`

- `std::jthread` 是 C++20 提供的 RAII 线程封装。
- 它会在析构时自动 `join`，能显著减少忘记回收线程的风险。
- 同时支持 `stop_token` 作为停止请求机制。

### 1.5 `this_thread` 工具

- `this_thread::sleep_for()`：让当前线程休眠一段时间。
- `this_thread::sleep_until()`：让当前线程休眠到指定时刻。
- `this_thread::get_id()`：获取当前线程 id。
- `this_thread::yield()`：提示调度器切换到其他可运行线程。

### 1.6 学到的关键点

- 线程函数的返回值不会自动保留。
- 想获得异步结果，应使用 `future / promise / async / packaged_task`。
- 多线程共享普通变量时，如果没有同步，会产生数据竞争。

---

## 2. 互斥量基础：`mutex` 家族

### 2.1 `std::mutex`

- 基础互斥量。
- 同一时刻只允许一个线程持有。
- 常用操作：
  - `lock()`
  - `try_lock()`
  - `unlock()`

注意：

- 同一线程重复 `lock` 普通 `mutex` 是未定义行为，通常表现为死锁。

### 2.2 `std::timed_mutex`

- 在 `mutex` 基础上支持超时加锁。
- 额外支持：
  - `try_lock_for()`
  - `try_lock_until()`

### 2.3 `std::recursive_mutex`

- 允许同一线程重复加锁。
- 需要对应次数的 `unlock()` 才会真正释放。
- 递归层数通常有实现限制。

### 2.4 `std::recursive_timed_mutex`

- `recursive_mutex + timed_mutex`。

### 2.5 `std::shared_mutex`

- 支持两种锁模式：
  - 独占锁
  - 共享锁

独占锁：

- `lock()`
- `try_lock()`
- `unlock()`

共享锁：

- `lock_shared()`
- `try_lock_shared()`
- `unlock_shared()`

特点：

- 多个读线程可以同时持有共享锁。
- 写线程持有独占锁时，其他读写都会被阻塞。
- 如果已有读锁，写锁需要等待。

### 2.6 学到的关键点

- 互斥量的作用是保护共享状态，避免数据竞争。
- 读多写少场景中，`shared_mutex` 比普通 `mutex` 更适合。

---

## 3. RAII 锁封装：`lock_guard` / `unique_lock` / `scoped_lock` / `shared_lock`

### 3.1 为什么要用 RAII 锁

手动写：

```cpp
mtx.lock();
// ...
mtx.unlock();
```

有几个问题：

- 容易忘记解锁
- 异常路径容易漏解锁
- 提前 `return` 时容易出错

RAII 锁对象把“加锁”放在构造函数，把“解锁”放在析构函数中，离开作用域自动释放，更安全。

### 3.2 `std::lock_guard`

- 最简单的作用域锁。
- 构造时加锁，析构时解锁。
- 不支持移动，不支持拷贝。
- 适合“进入临界区后一直持有到作用域结束”的场景。

等价思想：

```cpp
template<class Mutex>
class lock_guard {
    Mutex& m;
public:
    explicit lock_guard(Mutex& m_) : m(m_) { m.lock(); }
    ~lock_guard() { m.unlock(); }
};
```

### 3.3 一个非常重要的坑：临时锁对象

错误写法：

```cpp
lock_guard<mutex>{mtx};
```

这会创建一个临时对象，它的生命周期只到当前完整表达式结束。也就是说，它会立刻析构，后面的代码实际上是无锁运行。

结论：

- 锁对象必须绑定到一个具名变量上。

### 3.4 `std::unique_lock`

- 比 `lock_guard` 更灵活。
- 支持移动，不支持拷贝。
- 可以延迟加锁、尝试加锁、超时加锁、手动解锁、转移所有权。

常见构造方式：

- `unique_lock<mutex> ul(mtx);`
- `unique_lock<mutex> ul(mtx, defer_lock);`
- `unique_lock<mutex> ul(mtx, try_to_lock);`
- `unique_lock<mutex> ul(mtx, adopt_lock);`

常见接口：

- `lock()`
- `try_lock()`
- `unlock()`
- `release()`
- `mutex()`
- `owns_lock()`
- `operator bool()`

### 3.5 `release()` 的含义

- `release()` 放弃“锁所有权”，但不会自动解锁底层互斥量。
- 调用后当前 `unique_lock` 变为空壳。
- 如果之后不手动解锁底层互斥量，就可能留下已上锁状态。

### 3.6 `std::scoped_lock`

- 用于同时管理多个互斥量。
- 会使用类似 `std::lock` 的死锁避免策略。
- 适合多把锁一起加锁的场景。

### 3.7 `std::shared_lock`

- 共享锁版本的 RAII 封装。
- 适用于 `shared_mutex` 的读锁管理。

### 3.8 锁策略标签

- `defer_lock`
- `try_to_lock`
- `adopt_lock`

它们本质是标签对象，用来指定加锁策略。

### 3.9 学到的关键点

- 能用 RAII 锁就尽量不用手写 `lock/unlock`。
- `condition_variable` 通常要求配合 `unique_lock` 使用。
- 多把锁不要自己按固定顺序乱写，优先用 `scoped_lock` 或 `std::lock`。

---

## 4. 单次初始化：`std::call_once` 与单例

### 4.1 `std::call_once`

作用：

- 保证某段初始化代码在多线程环境下只执行一次。

配套对象：

- `std::once_flag`

使用形式：

```cpp
std::once_flag flag;
std::call_once(flag, func, args...);
```

### 4.2 语义特点

- 第一次调用时执行目标函数。
- 后续调用直接返回，不会重复执行。
- 如果目标函数抛异常，`once_flag` 不会被标记为完成。
- 后续调用仍会再次尝试执行。

### 4.3 与 `std::thread` 参数传递的区别

- `call_once` 在当前线程立即执行，不像 `std::thread` 那样需要把参数转移到新线程。
- 因此它不像线程启动那样强调“拷贝/移动到新执行上下文”。

### 4.4 单例写法

你学习了两种单例思路：

1. `call_once + static unique_ptr`
2. 函数内局部静态对象

第二种通常更简单：

```cpp
static Singleton instance;
```

C++11 起，局部静态对象初始化具备线程安全保证。

### 4.5 学到的关键点

- `call_once` 很适合做全局资源初始化。
- 现代 C++ 中，局部静态单例往往比手写双重检查锁更简单、更安全。

---

## 5. 条件变量：`condition_variable` / `condition_variable_any`

### 5.1 条件变量的本质

条件变量不是“条件”本身，而是“等待队列 + 唤醒机制”。

它解决的问题是：

- 一个线程等待某个条件成立
- 另一个线程修改状态后通知它继续运行

### 5.2 `condition_variable` 必须搭配什么

- 必须搭配互斥量一起使用
- 标准接口要求配合 `std::unique_lock<std::mutex>`

### 5.3 `wait()` 的关键语义

```cpp
cv.wait(lk);
```

这个过程不是简单的“先解锁再睡眠”，而是原子性地完成：

1. 释放锁
2. 将当前线程挂入等待队列
3. 阻塞线程
4. 被唤醒后重新竞争并拿回锁
5. `wait()` 返回

这里最核心的价值是：

- 不会在“解锁”和“进入等待”之间出现竞态窗口。

### 5.4 为什么必须用谓词版 `wait`

标准推荐写法：

```cpp
cv.wait(lk, pred);
```

等价于：

```cpp
while (!pred()) {
    cv.wait(lk);
}
```

原因有两个：

- 防止虚假唤醒
- 防止条件在被唤醒后又被别的线程改回去

### 5.5 虚假唤醒

含义：

- 线程从 `wait()` 返回了，但并不表示条件一定成立。

这不是 bug，而是条件变量允许的正常行为。

解决办法：

- 永远在循环或谓词里检查条件。

### 5.6 丢失唤醒

含义：

- 一个线程本该收到通知，却因为时序竞争而永久睡死。

根源：

- 条件判断、进入等待、条件修改、发送通知，没有被同一把锁正确保护。

解决办法：

- 用同一把 `mutex` 保护共享状态。
- 用 `wait(lock, pred)` 保证“检查条件 + 入睡”之间没有竞态漏洞。

### 5.7 `notify_one` 与 `notify_all`

- `notify_one()`：唤醒一个等待线程
- `notify_all()`：唤醒所有等待线程

注意：

- 被唤醒不等于立刻运行
- 它们仍然需要重新竞争互斥锁

### 5.8 通知时机

通常推荐：

1. 先持锁修改条件
2. 解锁
3. 再 `notify_one()` / `notify_all()`

这样可以减少“线程刚被唤醒又因为抢不到锁继续阻塞”的额外竞争。

### 5.9 `condition_variable_any`

- 更通用
- `wait` 是模板形式
- 可配合任意满足 `BasicLockable` 的锁类型
- 可以和 `shared_lock<shared_mutex>` 配合使用

### 5.10 `notify_all_at_thread_exit`

- 在线程彻底退出前发出通知
- 它和普通 `unlock + notify` 的差别在于：通知发生在线程局部对象销毁之后

### 5.11 学到的关键点

- 条件变量不保存状态，状态在你自己维护的共享变量里。
- `mutex` 保护的是“条件状态”，`condition_variable` 管理的是“等待线程队列”。
- 写法上优先使用 `wait(lock, pred)`。

---

## 6. 生产者消费者模型

### 6.1 问题本质

生产者消费者是并发编程的经典模型：

- 生产者并行准备任务
- 在临界区内把任务放入共享队列
- 消费者在临界区内取任务
- 在临界区外处理任务

### 6.2 共享状态

典型共享状态包括：

- 任务队列
- 队列容量
- 停止标志
- 活跃生产者数量

这些状态都必须受同一把锁保护。

### 6.3 两个条件变量的意义

常见做法是两个条件变量：

- `cv_not_full`：队列未满时唤醒生产者
- `cv_not_empty`：队列非空时唤醒消费者

这样等待谓词更清晰。

### 6.4 正确退出的关键

退出不是简单靠“多发几个通知”实现的，而是依赖正确的退出条件设计。

你这里总结出的退出逻辑很重要：

1. 主线程先设置“停止继续生产”
2. 生产者陆续退出
3. 最后一个生产者通知消费者“不会再有新任务”
4. 消费者继续处理剩余任务
5. 当“所有生产者都退出且队列为空”时，消费者结束

### 6.5 为什么这很重要

这说明你已经不只是会写 `cv.wait`，而是在理解：

- 等待谓词到底该包含什么
- 系统如何“安全停机”
- 如何区分“暂时没任务”和“永远不会再有任务”

### 6.6 两种实现风格

你已经写了两版：

1. 全局变量版
2. `TaskQueue` 类封装版

第二种更工程化，因为：

- 状态封装更清晰
- 同步逻辑集中
- 更利于复用和维护

### 6.7 学到的关键点

- 并发代码不仅要“运行起来”，还要考虑退出协议。
- `wait` 的谓词必须覆盖“继续等待”和“允许退出”两种状态。

---

## 7. 异步结果通道：`promise` / `future` / `shared_future`

### 7.1 核心模型：`shared_state`

`promise` 和 `future` 不是直接互相连接的，它们共享一个控制块，通常称为 `shared_state`。

其中保存：

- 结果值
- 异常
- 就绪状态

### 7.2 `std::promise`

作用：

- 生产异步结果

特点：

- 不支持拷贝
- 支持移动

常用操作：

- `get_future()`
- `set_value()`
- `set_exception()`
- `set_value_at_thread_exit()`
- `set_exception_at_thread_exit()`

### 7.3 `promise` 的重要异常语义

如果 `promise` 析构时共享状态还未就绪：

- 会自动向共享状态写入 `broken_promise` 异常

这点很重要，因为它解释了：

- 为什么等待方不会永远卡死
- 为什么 `future.get()` 可能抛异常

### 7.4 `std::future`

作用：

- 等待并获取异步结果

特点：

- 是结果的唯一接收端
- 不支持拷贝
- 支持移动

常见接口：

- `valid()`
- `wait()`
- `wait_for()`
- `wait_until()`
- `get()`
- `share()`

### 7.5 `future::get()` 的语义

- 阻塞直到结果就绪
- 如果共享状态里保存的是异常，则重新抛出
- 调用 `get()` 后，`future` 失效，`valid() == false`

### 7.6 `shared_future`

作用：

- 多个接收方共享同一个异步结果

特点：

- 可复制
- 可多次 `get()`
- 调用 `get()` 后不会失效

### 7.7 学到的关键点

- `promise` 是写端，`future` 是读端。
- `future` 适合“单消费者”结果传递。
- `shared_future` 适合“广播式读取”。

---

## 8. 任务包装与高级异步：`packaged_task` / `async`

### 8.1 `std::packaged_task`

作用：

- 把一个可调用对象包装成“执行后自动把结果写入共享状态”的任务对象。

本质上它连接了两件事：

1. 函数调用
2. `future` 结果获取

### 8.2 典型流程

```cpp
packaged_task<R()> task(f);
future<R> fu = task.get_future();
// 某处执行 task();
// 另一个地方 fu.get();
```

### 8.3 适用场景

- 线程池任务
- 任务队列
- 把普通函数调用接入 future 模型

### 8.4 注意点

- `packaged_task` 不可拷贝，只能移动
- 每个任务的共享状态只能 `get_future()` 一次
- 任务执行多次会报错

### 8.5 `std::async`

作用：

- 异步运行函数，并直接返回一个 `future`

调用形式：

```cpp
auto fu = std::async(f, args...);
auto fu = std::async(std::launch::async, f, args...);
auto fu = std::async(std::launch::deferred, f, args...);
```

### 8.6 启动策略

#### `std::launch::async`

- 立即异步执行
- 通常创建新线程

#### `std::launch::deferred`

- 不立即执行
- 直到第一次 `wait()` / `get()` 时才执行
- 执行线程就是调用 `wait/get` 的那个线程

#### 默认策略

```cpp
std::launch::async | std::launch::deferred
```

实现可以自行选择策略。

### 8.7 一个容易忽略的点

如果 `std::async` 返回的 `future` 是一个临时对象，没有被保存：

- 在完整表达式结束时，`future` 析构可能阻塞等待异步任务完成

于是原本想写异步，结果实际效果接近同步。

### 8.8 学到的关键点

- `async` 是最方便的高级异步接口。
- 但它的策略选择、析构阻塞行为，都必须理解清楚。
- 想精确控制执行位置和任务时机时，`packaged_task + 线程/线程池` 更灵活。

---

## 9. 综合例子：基于 `packaged_task` 的任务队列

### 9.1 模型结构

你最后实现的是一个很有代表性的并发模型：

1. 提交方提交任务
2. 任务进入共享队列
3. 工作线程取任务执行
4. 提交方通过 `future` 收集结果

### 9.2 这个例子融合了哪些知识点

- `mutex`
- `unique_lock`
- `condition_variable`
- `packaged_task`
- `future`
- 工作线程循环
- 有界队列
- 安全停机

### 9.3 为什么这个例子重要

因为它已经接近线程池/任务调度器的核心结构了。

虽然现在还是一个简化版本，但你已经真正把以下概念串起来了：

- 同步共享队列
- 唤醒工作线程
- 异步返回结果
- 生产端与消费端解耦

这比单纯记 API 更有价值。

---

## 10. 当前阶段已经掌握的主线

到目前为止，你已经形成了这样一条并发知识链路：

1. `thread / jthread`：创建并管理线程
2. `mutex` 家族：保护共享状态
3. RAII 锁封装：安全持锁
4. `call_once`：一次性初始化
5. `condition_variable`：线程等待与唤醒
6. 生产者消费者：经典同步模型
7. `promise / future`：异步结果传递
8. `packaged_task / async`：更高级的异步执行封装
9. 任务队列：综合应用

这条线已经相当完整，说明你并不是只学了零散 API，而是在逐步建立 C++ 并发编程的整体框架。

---

## 11. 目前最重要的易错点总结

### 11.1 线程参数默认按值传递

- 引用参数要用 `std::ref`
- 同时保证被引用对象活得够久

### 11.2 `std::thread` 析构前必须处理

- 要么 `join()`
- 要么 `detach()`
- 否则 `terminate()`

### 11.3 普通共享变量并发访问会有数据竞争

- 尤其是至少一个线程写时
- 这是未定义行为

### 11.4 不要把 RAII 锁写成临时对象

- 临时 `lock_guard` 会立刻析构
- 后续代码可能根本没被锁保护

### 11.5 `condition_variable` 不保存条件

- 条件必须由共享变量表达
- 条件变量只负责等待队列和通知

### 11.6 必须使用谓词版 `wait`

- 防虚假唤醒
- 防止逻辑漏洞

### 11.7 `notify` 不等于立即运行

- 被唤醒线程还需要重新抢锁

### 11.8 `future.get()` 只能消费一次

- 调用后 `future` 失效
- 多消费者场景用 `shared_future`

### 11.9 `promise` 未兑现会变成异常

- 析构时如果共享状态未就绪，会形成 `broken_promise`

### 11.10 `packaged_task` 和很多并发组件一样不可拷贝

- 线程启动时如果内部要保存对象，就要注意用 `std::ref` 或移动

### 11.11 `async` 的默认策略不是“必定起新线程”

- 可能延迟执行
- 要明确区分 `async` 和 `deferred`

---

## 12. 你现在处于什么阶段

如果从学习阶段看，你已经基本完成了：

- 并发基础线程管理
- 基础同步原语
- 经典条件同步
- 异步结果机制
- 简单任务调度模型

也就是说，你现在已经从“会开线程”进入了“理解线程间协作”的阶段。

这时候下一步最自然的方向就是：

- `std::atomic`
- `compare_exchange`
- `memory_order` 基础

因为你前面已经把：

- 互斥
- 条件同步
- 异步结果传递

这些高层机制搭好了，接下来可以往更底层的无锁同步原语过渡。

---

## 13. 建议的下一步学习顺序

结合你现在的基础，建议接下来这样学：

1. 先理解什么是 `data race`
2. 学 `std::atomic<T>` 的基本语义
3. 学 `load / store / fetch_add / exchange`
4. 学 `compare_exchange_weak / strong`
5. 只在默认 `seq_cst` 下理解原子操作
6. 再进入 `memory_order_acquire / release`
7. 之后再看 `relaxed`
8. 最后再补更复杂的话题，例如 `atomic_wait/notify`、`fence`

---

## 14. 一句话总结

你现在已经完成了 C++ 并发里“线程 + 锁 + 条件变量 + future 模型 + 任务队列”这一整层内容。下一步不该再反复背这些 API，而是应该开始进入 `atomic` 和并发内存模型，逐步建立更底层的理解。