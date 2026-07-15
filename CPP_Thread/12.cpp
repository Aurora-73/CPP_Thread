
// 对比项目中的无锁数据结构：
//
// 项目 concurrent/day17~18 实现了三种栈 + 一种队列：
//   1. lock_free_stack     — 原始指针 + 延迟删除（to_be_deleted 链表）
//   2. hazard_pointer_stack — Hazard Pointer 全局数组保护
//   3. ref_count_stack      — split reference count（外部+内部计数）
//   4. lock_free_queue      — Michael-Scott 队列，原始指针 + split reference count
//   5. CircularQueSync      — MPSC 有界环形队列，三原子变量（head/tail/tail_update）
//
// 你的实现用 C++20 atomic<shared_ptr> 统一解决了 ABA + 内存回收，代码量少一个数量级。
// 但有两个需要注意的点：
//
// 1. atomic<shared_ptr> 在 libstdc++ 中内部用 mutex 实现，不是真正 lock-free。
//    项目里的原始指针版本才是真正的 lock-free，适合面试讨论底层原理。
//
// 2. LockFreeStack::pop() 存在数据竞争：CAS 成功后访问 old_head->data 可能读到
//    已被其他线程 move 走的值。下面已修复。
//
// 3. SPSCRingQueue::size() 不保证原子读取（head 和 tail 分别 load，中间可能被更新），
//    用于精确计数时需注意。对于 "是否为空/满" 的判断足够了。

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <thread>
#include <vector>

using namespace std;

// 实现无锁链栈，无锁链队列，无锁循环队列

/* 无锁栈 (C++20 atomic<shared_ptr>)
不提供 top 函数，因为多线程这个操作不安全，仅提供 pop 和 try_pop */

template <typename T>
class LockFreeStack {
public:
	LockFreeStack() = default;

	~LockFreeStack() {
		shared_ptr<Node> curr = head.exchange(nullptr);
		while(curr) {
			curr = std::move(curr->next);
		}
	}

	LockFreeStack(const LockFreeStack &) = delete;
	LockFreeStack &operator=(const LockFreeStack &) = delete;

	void push(T value) {
		shared_ptr<Node> new_node = make_shared<Node>(std::move(value));
		new_node->next = head.load();
		while(!head.compare_exchange_weak(new_node->next, new_node));
	}

	optional<T> pop() {
		shared_ptr<Node> old_head = head.load();
		while(old_head) {
			// BUG FIX: 必须在 CAS 之前拷贝 data。
			// CAS 成功后 old_head 不再受栈保护，其他线程可能已经 move 走了 data。
			T data_copy = old_head->data;
			if(head.compare_exchange_weak(old_head, old_head->next)) {
				return std::move(data_copy);
			}
			// CAS 失败时 old_head 已被自动更新为最新值，重试
		}
		return nullopt;
	}

	// 快照判空
	bool empty() const {
		return head.load() == nullptr;
	}

private:
	struct Node {
		T data;
		shared_ptr<Node> next; // 非 atomic，因为只会栈修改 head 指针，不需要全部 atomic
		Node(T d) : data(std::move(d)), next(nullptr) { }
	};

	atomic<shared_ptr<Node>> head;
};

/* Michael-Scott 无锁队列 (C++20 atomic<shared_ptr>)
 无锁: 所有操作均无阻塞, 基于 CAS 循环
 无 ABA: atomic<shared_ptr> 内部整合了引用计数, 天然防御 ABA
 无内存泄漏: shared_ptr 自动管理节点生命周期
 无递归析构: 迭代切断 next 链, 防止深链栈溢出 */

template <typename T>
class LockFreeQueue {
public:
	LockFreeQueue() {
		shared_ptr<Node> dummy = make_shared<Node>();
		head.store(dummy);
		tail.store(dummy);
	}

	~LockFreeQueue() {
		// 手动断开链表, 避免 shared_ptr 递归析构导致栈溢出
		shared_ptr<Node> curr = head.exchange(nullptr);
		tail.store(nullptr);
		while(curr) {
			curr = curr->next.exchange(nullptr);
		}
	}

	LockFreeQueue(const LockFreeQueue &) = delete;
	LockFreeQueue &operator=(const LockFreeQueue &) = delete;

	void push(T value) {
		shared_ptr<Node> new_node = make_shared<Node>(std::move(value));

		while(true) {
			shared_ptr<Node> t = tail.load();
			shared_ptr<Node> next = t->next.load();

			if(tail.load() != t) continue; // tail 被其他线程修改, 重读

			if(next == nullptr) {
				// 尝试将新节点链接到尾部
				if(t->next.compare_exchange_weak(next, new_node)) {
					// 帮助推进 tail (即使失败也没关系, 下次 push/pop 会帮忙推进)
					tail.compare_exchange_strong(t, new_node);
					return;
				}
			} else {
				// tail 落后了, 帮助推进
				tail.compare_exchange_strong(t, next);
			}
		}
	}

	// 非阻塞出队, 空队列返回 nullopt
	optional<T> try_pop() {
		while(true) {
			shared_ptr<Node> h = head.load();
			shared_ptr<Node> t = tail.load();
			shared_ptr<Node> next = h->next.load();

			if(head.load() != h) continue; // head 被其他线程修改, 重读

			if(h == t) {
				if(next == nullptr) return nullopt;    // 队列为空
				tail.compare_exchange_strong(t, next); // tail 落后, 帮助推进
			} else {
				// 尝试将 head 推进到下一个节点
				if(head.compare_exchange_strong(h, next)) {
					// next 一定非空：h != t 意味着队列非空，只有 tail 节点的 next 才可能为 null
					return std::move(*next->data);
				}
			}
		}
	}

	// 阻塞出队 (spin + yield, 空队列时让出 CPU)
	T pop() {
		while(true) {
			auto result = try_pop();
			if(result) return std::move(*result);
			std::this_thread::yield();
		}
	}

	// 快照判空 (返回的只是瞬间状态, 调用后可能立即变化)
	bool empty() const {
		return head.load()->next.load() == nullptr;
	}

private:
	struct Node {
		optional<T> data;              // dummy 节点 data=nullopt
		atomic<shared_ptr<Node>> next; // C++20 atomic 智能指针
		Node() : data(nullopt), next(nullptr) { }
		Node(T d) : data(std::move(d)), next(nullptr) { }
	};

	atomic<shared_ptr<Node>> head;
	atomic<shared_ptr<Node>> tail;
};

/* SPSC 有界无锁队列 (环形缓冲区)
 无 CAS: 纯 atomic load/store, 真正的 waitfree
 性能: O(1), 单次操作仅几个原子指令
 适用: 单生产者 → 单消费者, 最常见的高性能通道
 容量: 上取整为 2 的幂, 用位掩码替代取模 */

template <typename T>
class SPSCRingQueue {
public:
	explicit SPSCRingQueue(size_t capacity) {
		capacity_ = 1;
		while(capacity_ < capacity + 1) capacity_ <<= 1; // +1 区分满/空
		mask_ = capacity_ - 1;
		buffer_.resize(capacity_);
	}

	// 生产者入队, 满返回 false (非阻塞)
	bool push(T value) {
		size_t t = tail.load(memory_order_relaxed); // 只有生产者自己写 tail，无需同步
		size_t h = head.load(memory_order_acquire); // acquire 同步消费者的 release，确保看到已释放的槽位
		if(t - h >= capacity_) return false;

		buffer_[t & mask_] = std::move(value);
		tail.store(t + 1, memory_order_release); // release 发布数据，消费者 acquire 后可见
		return true;
	}

	// 消费者出队, 空返回 nullopt
	optional<T> try_pop() {
		size_t h = head.load(memory_order_relaxed); // 只有消费者自己写 head，无需同步
		size_t t = tail.load(memory_order_acquire); // acquire 同步生产者的 release，确保看到已发布的数据
		if(h == t) return nullopt;

		T value = std::move(buffer_[h & mask_]);
		head.store(h + 1, memory_order_release); // release 释放槽位，生产者 acquire 后可复用
		return value;
	}

	// 阻塞入队 (spin + yield)
	void blocking_push(T value) {
		while(!push(std::move(value))) {
			this_thread::yield();
		}
	}

	// 阻塞出队 (spin + yield)
	T blocking_pop() {
		while(true) {
			auto result = try_pop();
			if(result) return std::move(*result);
			this_thread::yield();
		}
	}

	bool empty() const {
		return head.load(memory_order_relaxed) == tail.load(memory_order_relaxed);
	}

	size_t size() const {
		return tail.load(memory_order_relaxed) - head.load(memory_order_relaxed);
	}

	size_t capacity() const {
		return capacity_;
	}

private:
	size_t capacity_;
	size_t mask_;
	vector<T> buffer_;   // 无锁: SPSC 下读写不同槽, 无竞争
	atomic<size_t> head; // 仅消费者写入
	atomic<size_t> tail; // 仅生产者写入
};

// 测试代码
namespace {

void test_ring_basic() {
	cout << "[SPSC Ring] basic ops... ";
	SPSCRingQueue<int> q(4);

	assert(q.empty());
	assert(q.capacity() == 8);
	assert(q.size() == 0);

	assert(q.push(1));
	assert(q.push(2));
	assert(q.push(3));
	assert(!q.empty());
	assert(q.size() == 3);

	auto v = q.try_pop();
	assert(v && *v == 1);
	v = q.try_pop();
	assert(v && *v == 2);
	v = q.try_pop();
	assert(v && *v == 3);
	assert(q.try_pop() == nullopt);
	assert(q.empty());
	cout << "OK" << endl;
}

void test_ring_full() {
	cout << "[SPSC Ring] full rejection... ";
	SPSCRingQueue<int> q(2);

	assert(q.push(1));
	assert(q.push(2));
	assert(q.push(3));
	assert(q.push(4));
	assert(!q.push(5));

	assert(q.try_pop() == 1);
	assert(q.try_pop() == 2);
	assert(q.try_pop() == 3);
	assert(q.try_pop() == 4);
	assert(q.try_pop() == nullopt);
	cout << "OK" << endl;
}

void test_ring_spsc() {
	constexpr int N = 1'000'000;
	cout << "[SPSC Ring] producer-consumer (" << N << " items)... ";
	SPSCRingQueue<int> q(1024);
	auto t0 = chrono::steady_clock::now();

	thread producer([&]() {
		for(int i = 0; i < N; ++i) q.blocking_push(i);
	});

	atomic<long long> sum {0};
	thread consumer([&]() {
		for(int i = 0; i < N; ++i) {
			sum.fetch_add(q.blocking_pop(), memory_order_relaxed);
		}
	});

	producer.join();
	consumer.join();

	auto ms = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t0).count();

	assert(sum.load() == (long long)N * (N - 1) / 2);
	cout << "OK (" << ms << " ms, " << (N * 1000.0 / ms / 1'000'000) << " M ops/s)" << endl;
}

void test_stack_mt() {
	constexpr int PRODUCERS = 4;
	constexpr int CONSUMERS = 4;
	constexpr int N = 1'000'000;
	constexpr int TOTAL = PRODUCERS * N;

	cout << "[LockFreeStack] " << PRODUCERS << "P/" << CONSUMERS << "C (" << TOTAL << " items)... ";

	LockFreeStack<int> s;
	atomic<long long> sum_in {0}, sum_out {0};
	atomic<int> push_done {0};
	auto t0 = chrono::steady_clock::now();

	vector<thread> producers;
	for(int p = 0; p < PRODUCERS; ++p) {
		producers.emplace_back([&, p]() {
			mt19937 rng(p);
			for(int i = 0; i < N; ++i) {
				int v = (int)(rng() & 0x7FFFFFFF);
				sum_in.fetch_add(v, memory_order_relaxed);
				s.push(v);
			}
			push_done.fetch_add(1, memory_order_release);
		});
	}

	vector<thread> consumers;
	for(int c = 0; c < CONSUMERS; ++c) {
		consumers.emplace_back([&]() {
			int idle = 0;
			while(true) {
				auto v = s.pop();
				if(v) {
					sum_out.fetch_add(*v, memory_order_relaxed);
					idle = 0;
				} else {
					// 退出条件：所有 producer 完成 且 栈空。
					// 存在竞态窗口：push_done 已设置但其他 consumer 还在 pop 最后几个元素，
					// 此时当前 consumer 可能看到空栈。用 idle 计数重试来容错。
					if(push_done.load(memory_order_acquire) == PRODUCERS && s.empty()) {
						if(++idle >= 3) break;
					} else {
						idle = 0;
					}
					this_thread::yield();
				}
			}
		});
	}

	for(auto &t : producers) t.join();
	for(auto &t : consumers) t.join();

	auto ms = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t0).count();

	assert(sum_in.load() == sum_out.load());
	cout << "OK (" << ms << " ms, " << (TOTAL * 1000.0 / ms / 1'000'000) << " M ops/s)" << endl;
}

void test_queue_mt() {
	constexpr int PRODUCERS = 4;
	constexpr int CONSUMERS = 4;
	constexpr int N = 1'000'000;
	constexpr int TOTAL = PRODUCERS * N;

	cout << "[LockFreeQueue] " << PRODUCERS << "P/" << CONSUMERS << "C (" << TOTAL << " items)... ";

	LockFreeQueue<int> q;
	atomic<long long> sum_in {0}, sum_out {0};
	atomic<int> push_done {0};
	auto t0 = chrono::steady_clock::now();

	vector<thread> producers;
	for(int p = 0; p < PRODUCERS; ++p) {
		producers.emplace_back([&, p]() {
			mt19937 rng(p + 100);
			for(int i = 0; i < N; ++i) {
				int v = (int)(rng() & 0x7FFFFFFF);
				sum_in.fetch_add(v, memory_order_relaxed);
				q.push(v);
			}
			push_done.fetch_add(1, memory_order_release);
		});
	}

	vector<thread> consumers;
	for(int c = 0; c < CONSUMERS; ++c) {
		consumers.emplace_back([&]() {
			int idle = 0;
			while(true) {
				auto v = q.try_pop();
				if(v) {
					sum_out.fetch_add(*v, memory_order_relaxed);
					idle = 0;
				} else {
					// 退出条件：所有 producer 完成 且 队列空。
					// 同 test_stack_mt，用 idle 计数重试来容错竞态窗口。
					if(push_done.load(memory_order_acquire) == PRODUCERS && q.empty()) {
						if(++idle >= 3) break;
					} else {
						idle = 0;
					}
					this_thread::yield();
				}
			}
		});
	}

	for(auto &t : producers) t.join();
	for(auto &t : consumers) t.join();

	auto ms = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t0).count();

	assert(sum_in.load() == sum_out.load());
	cout << "OK (" << ms << " ms, " << (TOTAL * 1000.0 / ms / 1'000'000) << " M ops/s)" << endl;
}

}

int main() {
	cout << "=== Lock-Free Verification Tests ===" << endl;
	test_ring_basic();
	test_ring_full();
	test_ring_spsc();
	test_stack_mt();
	test_queue_mt();
	cout << "=== All tests passed ===" << endl;
	return 0;
}
