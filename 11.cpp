#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

// atomic 原子操作
/*
template <class T> struct atomic;
template <class U> struct atomic<U *>;
template <class U> struct atomic<std::shared_ptr<U>>;  (C++ 20 起)
template <class U> struct atomic<std::weak_ptr<U>>;  (C++ 20 起)

    atomic 定义一个原子类型，并针对布尔、整数、浮点数(C++20 起)和指针类型的特化。
    如果一个线程写入原子对象，同时另一线程从它读取，那么行为有良好定义。
    对原子对象的访问可以建立线程间同步，并按 std::memory_order 对非原子内存访问定序。

    atomic 的模版类型参数要求：可平凡复制 (TriviallyCopyable，通常等价于支持 memcpy)、支持拷贝和移动、不包含 const\volatile 修饰

    原子操作有以下类型：
        存储：写入
        加载： 读取
        读修改写（RMW）： 读取并写入，注：这是一个原子操作而不是数个

    构造函数：仅在 T 类型可默认构造的时候提供默认构造函数，atomic 对象本身不可拷贝、不可移动。

    void store( T desired, std::memory_order order = std::memory_order_seq_cst ) noexcept;
        以 desired 原子地替换当前值。按照 order 的值影响内存。order 合法取值是 relaxed、release、seq_cst，也就是不支持读语义的内存序

    T operator=( T desired ) noexcept;
        将 desired 原子地赋给原子变量。等价于 store(desired)，使用默认内存序。

    T load( std::memory_order order = std::memory_order_seq_cst ) const noexcept;
        原子地加载并返回原子变量的当前值。按照 order 的值影响内存。 order 的合法取值是 relaxed、acquire、seq_cst。

    operator T() const noexcept;
        原子地加载并返回原子变量的当前值。等价于 load()，使用默认内存序。

    T exchange( T desired, std::memory_order order = std::memory_order_seq_cst ) noexcept;
        以 desired 原子地替换底层值。操作为读-修改-写操作。根据 order 的值影响内存。返回原子变量在调用前的值。

    bool compare_exchange_weak(T &expected, T desired, std::memory_order success, std::memory_order failure) noexcept;
    bool compare_exchange_weak(T &expected, T desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
    bool compare_exchange_strong(T &expected, T desired, std::memory_order success, std::memory_order failure) noexcept;
    bool compare_exchange_strong(T &expected, T desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
        compare_exchange。本质是：逐位比较 *this 与 expected（不使用 operator==）：相等 → 以 desired 替换 *this（读-修改-写）； 不等 → 将 *this 的实际值写入 expected（加载） 但是必须原子化，不能中途被其他线程插入。
        原子地比较 *this 和 expected 的值。如果它们逐位相等，那么以 desired 替换前者（进行读修改写操作）。否则，将 *this 中的实际值加载进 expected（进行加载操作）。

        expected - 到期待在原子对象中找到的值的引用，失败时：expected = 实际值。
        desired - 在符合期待时存储到原子对象的值        order - 两个操作所用的内存同步定序
        success - 读修改写操作所用的内存同步定序        failure - 加载操作所用的内存同步定序
            如果 failure 强于 success 或者 是 std::memory_order_release 和 std::memory_order_acq_rel 之一，那么行为未定义
            CAS 成功路径是 读 + 写，而失败路径只有读，所以成功和失败的内存序可能不同

        成功更改底层原子值时返回 true，否则为返回 false。

        compare_exchange_strong 保证：只有真正不相等才失败。
		compare_exchange_weak 允许：即使相等也失败，即使当前值等于 expected 也可能返回 false。这是因为很多 CPU 架构无法保证条件写入一定成功。因此 weak 通常需要放在循环中使用。

    bool is_lock_free() const noexcept;
        检查此类型所有对象上的原子操作是否免锁。有些类型虽然满足 atomic 的要求，但是CPU 不支持对应的原子操作，实现可能会退化为内部加锁实现。

    std::atomic<T>::is_always_lock_free;
        常量表达式，若此原子类型始终为免锁则为 true，若它决不或有时为免锁则为 false。此常量的值与成员函数 is_lock_free 和非成员函数 std::atomic_is_lock_free 一致。

    void wait( T old, std::memory_order order = std::memory_order_seq_cst ) const noexcept; (C++ 20 起)
        进行原子等待操作。表现如同重复进行下列步骤：比较 this->load(order) 的值表示与 old。
        如果它们相等，那么阻塞直到 *this 被 notify_one() 或 notify_all() 提醒，或线程被虚假解除锁定。否则直接返回。
        这些函数保证只有 当前值 != old 才返回，即使底层实现发生了虚假解除锁定。
		(与cv不同，本身已经保存了状态，类似于 while(val == old) cv.wait(lock);，但是 wait 不会自己获取lock所有权)
        wait 的 order 合法取值是 relaxed、acquire、seq_cst。比较是逐位的（类似 std::memcmp）；不使用比较运算符。

    void notify_all() noexcept; (C++ 20 起)
    void notify_one() noexcept; (C++ 20 起)
        进行原子提醒操作。唤醒一个/多个正在等待的线程。

    为整数、浮点数和指针类型特化
        fetch_add  原子地将实参加到存储于原子对象的值上，并返回先前保有的值
        fetch_sub  原子地从存储于原子对象的值减去实参，并获得先前保有的值
        operator+=  与原子值进行加
        operator-=  与原子值进行减
    仅为整数和指针类型特化
        operator++  operator++(int)  令原子值增加一
        operator--  operator--(int)  令原子值减少一
    仅为整数类型特化
        fetch_and    fetch_or    fetch_xor    原子地进行实参和原子对象的值的逐位与、或、异或，并获得先前保有的值
        operator&=    operator|=    operator^=    与原子值进行逐位与、或、异或


内存序
	enum memory_order
	{
		memory_order_seq_cst,	// 默认，最强内存序，除 acquire/release 约束外，所有 seq_cst 原子操作还形成单一全局顺序。
		memory_order_relaxed,	// 仅保证原子性，不做额外约束
		memory_order_acquire,	// 获得语义。当前线程中此操作之后的读写，不能被重排到该操作之前。通常用于 load。（类比 mutex.lock()）
		memory_order_release,	// 释放语义。当前线程中此操作之前的读写，不能被重排到该操作之后。通常用于 store。（类比 mutex.unlock()）
		memory_order_acq_rel,	// 同时要求读取一致性和写入一致性
		memory_order_consume 	// C++26 弃用
	};


atomic_flag 是一种原子布尔类型。与所有 std::atomic 的特化不同，它保证是免锁的，且不提供加载或存储操作。
	构造函数
		atomic_flag() noexcept = default; 	平凡默认构造函数，初始化 std::atomic_flag 为未指定状态
		可以通过 std::atomic_flag v = ATOMIC_FLAG_INIT; 的形式初始化 std::atomic_flag 为清除状态的初始化器。
		不可移动，不可拷贝
	
	void clear( std::memory_order order = std::memory_order_seq_cst ) noexcept;
		将 std::atomic_flag 的状态原子地更改为清除（false）。按照 order 的值影响内存。

	bool test_and_set( std::memory_order order = std::memory_order_seq_cst ) noexcept;
		将 std::atomic_flag 的状态原子地更改为设置（true），并返回它先前保有的值。此操作是读-修改-写操作。按照 order 的值影响内存。

	bool test( std::memory_order order = std::memory_order_seq_cst ) const noexcept;
		原子地读取 *this 的值并返回该值。按照 order 的值影响内存。

	void wait( bool old, std::memory_order order = std::memory_order_seq_cst ) const noexcept;
		进行原子等待操作。 old 要检测 atomic_flag 的对象不再含有的值，order 强制的内存定序约束

    void notify_all() noexcept; (C++ 20 起)
    void notify_one() noexcept; (C++ 20 起)
        进行原子提醒操作。唤醒一个/多个正在等待的线程。


自旋锁
	class SpinLock {
	public:
		void lock() {
			while(flag.test_and_set(std::memory_order_acquire));
			// while(flag.exchange(true, std::memory_order_acquire));
		}

		void unlock() {
			flag.clear(std::memory_order_release);
			// flag.store(false, std::memory_order_release);
		}

	private:
		std::atomic_flag flag = ATOMIC_FLAG_INIT;
		// std::atomic<bool> flag {false}; // 等价写法，但是atomic<bool> 不一定保证无锁
	}; // 标准的自旋锁，会导致 CPU 空转，不建议使用用户态自旋锁

	class Mutex {
	public:
		void lock() {
			while(flag.test_and_set(std::memory_order_acquire)) {
				flag.wait(true);
			}
		}

		void unlock() {
			flag.clear(std::memory_order_release);
			flag.notify_one();
		}

	private:
		std::atomic_flag flag = ATOMIC_FLAG_INIT;
	}; // 带阻塞的自旋锁，竞争时会进入内核阻塞，效果类似 futex。

*/

using namespace std;

void pause() {
	cout << "Press Enter to continue..." << endl;
	cin.get();
}

template <typename T>
void is_lock_free_type(const char *name) {
	std::cout << std::boolalpha << "atomic<" << name << ">\t" << std::atomic<T>::is_always_lock_free << std::endl;
}
#define CHECK_LOCK_FREE(T) is_lock_free_type<T>(#T)

// 无锁栈 (C++20 atomic<shared_ptr>)
template <typename T>
class LockFreeStack {
public:
	LockFreeStack() = default;

	~LockFreeStack() {
		auto curr = head.load();
		head.store(nullptr); // 先清空 head，防止 atomic 析构时重复释放
		while(curr) {
			curr = std::move(curr->next);
		} // 自己手动实现迭代的 shared_ptr 断链，防止自动的递归析构导致栈溢出错误
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
		while(old_head && !head.compare_exchange_weak(old_head, old_head->next));
		if(!old_head) return nullopt;
		return std::move(old_head->data);
	} // atomic<shared_ptr> 的引用计数提供了天然的 ABA 保护（节点不会在持有引用时被释放复用）

	// 快照判空
	bool empty() const {
		return head.load() == nullptr;
	}

private:
	struct Node {
		T data;
		shared_ptr<Node> next; // 非 atomic: 由 head CAS 保护
		Node(T d) : data(std::move(d)), next(nullptr) { }
	};

	atomic<shared_ptr<Node>> head;
};

// 带阻塞的自旋锁，竞争时会进入内核阻塞类似 futex。
class Mutex {
public:
	void lock() noexcept {
		while(flag_.test_and_set(std::memory_order_acquire)) {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
			// C++ 20 起可以仅在 unlock 中通知后才获得锁，从而避免任何无效自旋
			// 注意，即使 wait 保证一定在值被更改后才返回，但锁定是在下一次执行条件时完成的
			flag_.wait(true, std::memory_order_relaxed);
#endif
		}
	}

	bool try_lock() noexcept {
		return !flag_.test_and_set(std::memory_order_acquire);
	}

	void unlock() noexcept {
		flag_.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		flag_.notify_one();
#endif
	}

private:
	std::atomic_flag flag_ {};
};

int main() {

	{
		cout << "string 类型" << (__is_trivially_copyable(string) ? "" : "不") << "是可平凡复制的" << endl;
		// atomic<string> 会报错，因为 string 不是可平凡复制的

		struct A {
			char a[8];
		};
		struct B {
			int b[100];
		};

		std::cout << "类型\t\t是否无锁\n";
		CHECK_LOCK_FREE(int);
		CHECK_LOCK_FREE(double);
		CHECK_LOCK_FREE(char);
		CHECK_LOCK_FREE(bool);
		CHECK_LOCK_FREE(char *);
		CHECK_LOCK_FREE(A);
		CHECK_LOCK_FREE(B);
		CHECK_LOCK_FREE(std::weak_ptr<int>);
		CHECK_LOCK_FREE(std::shared_ptr<std::string>);
	}

	pause();

	{
		atomic<int> value = 0;
		auto fetch_add = [](atomic<int> &value, int adder, int n) {
			for(int i = 0; i < n; ++i) {
				int oldValue = value.load();
				int desired;
				do {
					desired = oldValue + adder;
				} while(!value.compare_exchange_weak(oldValue, desired)); // CAS 循环
			}
		};

		vector<thread> ths;
		for(int i = 0; i < 10; ++i) {
			ths.emplace_back(fetch_add, std::ref(value), i, 100000);
		}
		for(auto &th : ths) th.join();
		cout << value.load() << endl;
	}

	pause();

	{
		LockFreeStack<int> stack;

		// 多线程 push (先完成 push 再开始 pop)
		{
			vector<jthread> producers;
			constexpr int N = 1000;
			for(int i = 0; i < 4; ++i) {
				producers.emplace_back([&stack, i]() {
					for(int j = 0; j < N; ++j) {
						stack.push(i * N + j);
					}
				});
			}
		}

		cout << "push done, empty=" << boolalpha << stack.empty() << endl;

		// 多线程 pop
		atomic<int> pop_count {0};

		{
			vector<jthread> consumers;
			for(int i = 0; i < 4; ++i) {
				consumers.emplace_back([&stack, &pop_count]() {
					while(stack.pop()) {
						pop_count.fetch_add(1, memory_order_relaxed);
					}
				});
			}
		}

		cout << "pop_count=" << pop_count.load() << endl;
		cout << "empty=" << stack.empty() << endl;
	}

	pause();

	{
		auto my_lock = [](int &a, Mutex &mtx) {
			lock_guard<Mutex> lg(mtx);
			for(int i = 0; i < 1000000; ++i) {
				a += 1;
			}
		};
		int a = 0;
		Mutex mtx;
		{
			vector<jthread> ths;
			for(int i = 0; i < 8; ++i) {
				ths.emplace_back(std::ref(my_lock), std::ref(a), std::ref(mtx));
			}
		}
		cout << a << endl;
	}

	return 0;
}
