#pragma once

// ============================================================
//  Thread-Safe Data Structures (C++17)
// ============================================================
//
//  1. SafeStack<T>        — 线程安全栈，mutex 保护的链表 LIFO
//  2. SafeQueue<T>        — 线程安全队列，Michael-Scott 算法思路 + mutex
//  3. SPSCRingQueue<T>    — SPSC 有界环形队列，纯 atomic load/store（wait-free）
//
//  栈和队列用 mutex + shared_ptr 管理节点生命周期：
//    - 代码简洁，无 use-after-free / ABA 问题
//    - shared_ptr 原子引用计数保证节点不会被意外释放
//    - 适合面试讲解算法思路
//
//  真正 lock-free 的栈/队列需要 CAS + Hazard Pointer 或 split reference count。
// ============================================================

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

// ==================== 线程安全栈 ====================
//  单向链表实现的 LIFO 栈。
//  push: 头插法，新节点的 next 指向旧 head，然后更新 head。
//  pop:  取出栈顶数据，head 前进到 next。
// =================================================

template <typename T>
class SafeStack {
public:
    SafeStack() = default;

    SafeStack(const SafeStack &) = delete;
    SafeStack &operator=(const SafeStack &) = delete;

    // 头插法：新节点 → 旧 head → ... → nullptr
    void push(T data) {
        auto new_node = std::make_shared<node>(std::move(data));
        std::lock_guard<std::mutex> lk(mtx);
        new_node->next = head;
        head = new_node;
    }

    // 取出栈顶数据，空栈返回 nullptr
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> lk(mtx);
        if(!head) return nullptr;
        auto res = std::make_shared<T>(std::move(head->data));
        head = head->next; // head 前进到下一个节点，旧节点由 shared_ptr 自动释放
        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return !head;
    }

private:
    struct node {
        T data;
        std::shared_ptr<node> next;
        node(T d) : data(std::move(d)) { }
    };

    mutable std::mutex mtx;       // mutable: const 方法（empty）也能加锁
    std::shared_ptr<node> head;   // 栈顶，nullptr 表示空栈
};

// ==================== 线程安全队列 ====================
//  mutex 保护的 FIFO 队列。
//  基于 Michael-Scott 队列的 dummy 哨兵思路，但用 mutex 简化了实现：
//    - 无需 CAS 和帮助推进机制
//    - 无需处理 ABA 和 use-after-free
//    - 保留了 dummy 节点设计，便于与真正的 lock-free 版本对比学习
//
//  关键点：
//    - dummy 节点始终存在，head 指向的节点不存数据
//    - push 在 tail 上存数据，然后链接新的 dummy 并推进 tail
//    - pop 从 head->next 读数据（跳过 dummy），清空数据使之成为新 dummy，推进 head
//    - head == tail 且 data 为空时，队列为空
// =============================================================

template <typename T>
class SafeQueue {
public:
    // 构造：创建唯一的 dummy 哨兵节点，head 和 tail 都指向它
    SafeQueue() {
        auto dummy = std::make_shared<node>();
        head = dummy;
        tail = dummy;
    }

    SafeQueue(const SafeQueue &) = delete;
    SafeQueue &operator=(const SafeQueue &) = delete;

    // 入队：在 tail 节点上存放数据，链接新的 dummy，推进 tail
    void push(T value) {
        auto new_data = std::make_shared<T>(std::move(value));
        auto new_dummy = std::make_shared<node>();
        std::lock_guard<std::mutex> lk(mtx);
        tail->data = new_data;      // 1. 在当前 tail（dummy）上存数据
        tail->next = new_dummy;     // 2. 链接新的 dummy 节点
        tail = new_dummy;           // 3. 推进 tail 到新 dummy
    }

    // 出队：跳过 dummy 节点，取出数据，推进 head
    std::optional<T> pop() {
        std::lock_guard<std::mutex> lk(mtx);
        // head 指向 dummy（data 为空），需要先前进到数据节点
        if(!head->data) {
            if(!head->next) return std::nullopt; // head->next 为空说明队列真正为空
            head = head->next;                    // 跳过 dummy，前进到数据节点
        }
        if(!head->data) return std::nullopt;     // 安全检查
        auto data = std::move(*head->data);
        head = head->next;                       // 推进 head 到下一个 dummy，旧节点由 shared_ptr 回收
        return data;
    }

    // 判断队列是否为空
    // head == tail：队列空（两个都指向尾部 dummy，data 为空）
    // head != tail 但 head->data 为空且 head->next == tail：
    //   pop 最后一个元素后 head 停在中间节点（data 已清空），tail 在末尾 dummy
    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        if(head == tail) return true;
        return !head->data && head->next == tail;
    }

private:
    struct node {
        std::shared_ptr<T> data;    // 数据域，dummy 节点为 nullptr
        std::shared_ptr<node> next; // 下一个节点
    };

    mutable std::mutex mtx;
    std::shared_ptr<node> head;     // 队头（dummy 节点，不存数据）
    std::shared_ptr<node> tail;     // 队尾（dummy 节点，不存数据）
};

// ==================== SPSC 有界无锁环形队列 ====================
//  Single-Producer Single-Consumer 有界环形缓冲区。
//  真正的 wait-free：无 CAS，无 mutex，纯 atomic load/store。
//
//  核心原理：
//    - 生产者只写 tail_，消费者只写 head_，无写-写竞争
//    - 用 memory_order_release/acquire 建立 happens-before 关系：
//      生产者写数据 → release tail_ → 消费者 acquire tail_ → 读数据
//    - 容量取 2 的幂，用位掩码 (index & mask_) 替代取模 (index % capacity_)
//
//  适用场景：高频 IPC、日志管道、Disruptor 模式
//
//  head_ 和 tail_ 用 alignas(64) 避免伪共享（false sharing）：
//  如果 head_ 和 tail_ 在同一缓存行，生产者写 tail_ 会使消费者缓存行失效，
//  反之亦然，导致大量不必要的缓存同步开销。64 字节对齐确保它们在不同缓存行。
// =============================================================

template <typename T>
class SPSCRingQueue {
public:
    // capacity + 1 再向上取整为 2 的幂，保证 next_power_of_2(capacity+1) >= capacity
    // 例如请求 capacity=4 → capacity_=8，可用 8 个槽位
    // 区分满和空：t - h == capacity_ 为满，t - h == 0 为空（SPSC 下无歧义）
    explicit SPSCRingQueue(size_t capacity) {
        capacity_ = 1;
        while(capacity_ < capacity + 1) capacity_ <<= 1;
        mask_ = capacity_ - 1; // 位掩码：capacity_=8 时 mask_=0b111
        buffer_.resize(capacity_);
    }

    // 生产者入队，满返回 false（非阻塞）
    bool push(const T &value) {
        size_t t = tail_.load(std::memory_order_relaxed); // 只有生产者写 tail_，无需同步
        size_t h = head_.load(std::memory_order_acquire); // acquire 消费者的 release，看到已释放的槽位
        if(t - h >= capacity_) return false;               // 缓冲区满

        buffer_[t & mask_] = value;                        // & mask_ 等价于 % capacity_，但更快
        tail_.store(t + 1, std::memory_order_release);     // release 发布数据，消费者 acquire 后可见
        return true;
    }

    // 消费者出队，空返回 nullopt（非阻塞）
    std::optional<T> try_pop() {
        size_t h = head_.load(std::memory_order_relaxed); // 只有消费者写 head_，无需同步
        size_t t = tail_.load(std::memory_order_acquire); // acquire 生产者的 release，看到已发布的数据
        if(h == t) return std::nullopt;                    // 缓冲区空

        T value = std::move(buffer_[h & mask_]);
        head_.store(h + 1, std::memory_order_release);    // release 释放槽位，生产者 acquire 后可复用
        return value;
    }

    // 阻塞版本：忙等 + yield，适合延迟敏感场景
    void blocking_push(const T &value) {
        while(!push(value)) std::this_thread::yield();
    }

    T blocking_pop() {
        for(;;) {
            auto result = try_pop();
            if(result) return *result;
            std::this_thread::yield();
        }
    }

    bool empty() const {
        return head_.load(std::memory_order_relaxed) ==
               tail_.load(std::memory_order_relaxed);
    }

    // 注意：size() 不保证原子一致性（head_ 和 tail_ 分别 load），
    // 用于精确计数时需要额外同步，但对"是否为空/满"的判断足够
    size_t size() const {
        return tail_.load(std::memory_order_relaxed) -
               head_.load(std::memory_order_relaxed);
    }

    size_t capacity() const { return capacity_; }

private:
    size_t capacity_;       // 总槽位数（2 的幂）
    size_t mask_;           // 位掩码 capacity_ - 1
    std::vector<T> buffer_; // 环形缓冲区

    // alignas(64) 确保 head_ 和 tail_ 在不同缓存行，避免伪共享
    // 生产者和消费者分别只写自己那一端，缓存行分离后互不干扰
    alignas(64) std::atomic<size_t> head_ = 0; // 消费者读位置
    alignas(64) std::atomic<size_t> tail_ = 0; // 生产者写位置
};
