#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

using namespace std;

// 通用互斥锁管理（RAII）
/*
注意：C++中临时对象（未命名的变量）的生命周期截止到完整表达式结束！！！！

lock_guard  实现严格基于作用域的互斥体所有权包装器 (类模板)
    构造函数：
        explicit lock_guard( mutex_type& m ); 引用m为成员变量，并调用 m.lock()。
        lock_guard( mutex_type& m, std::adopt_lock_t t ); 引用m为成员变量，不调用 m.lock()。
        支持移动构造函数，不支持拷贝构造函数
    析构函数：
        释放所拥有互斥量的所有权。实际调用 m.unlock()。销毁构对象
    等价写法：
        template<class Mutex>
        class lock_guard {
            Mutex& m;
        public:
            explicit lock_guard(Mutex& m_) : m(m_) { m.lock(); }
            ~lock_guard() { m.unlock(); }
        };

scoped_lock  用于多个互斥体的死锁避免 RAII 包装器 (类模板)
    创建 scoped_lock 对象时，它会尝试获取它所给定的互斥体的所有权。
    当控制离开创建 scoped_lock 对象的范围时，scoped_lock 会被销毁，并且互斥体会被释放。
    如果给定多个互斥体，则会使用死锁避免算法，如同通过 std::lock

unique_lock  实现可移动的互斥体所有权包装器 (类模板)
    有两个关键成员（概念上）： mutex_type *pmutex; bool owns;
    构造函数：
        unique_lock()： pmutex = nullptr，owns = false
        explicit unique_lock( mutex_type& m )： 关联 m 并立即加锁，owns = true
        unique_lock( mutex_type& m, std::defer_lock_t )： 关联 m，但不加锁，owns = false
        unique_lock( mutex_type& m, std::try_to_lock_t )： 关联 m，并尝试加锁，owns = pmutex->try_lock()
        unique_lock( mutex_type& m, std::adopt_lock_t )： 接管一个已经持有的锁，owns = true
        unique_lock( mutex_type& m, const chrono::time_point<...>& ) / const chrono::duration<...>&： 超时尝试加锁
    支持移动构造和移动赋值，不支持拷贝；移动后原对象变为空壳（pmutex = nullptr, owns = false）
    operator=：
        通过移动转移所有权；若当前对象持有锁，会先释放原锁，再接管新对象的状态
    析构函数：
        if(owns) pmutex->unlock();
    lock：
        对关联互斥量加锁；pmutex->lock(); owns = true; 若未关联或已持有锁，会抛异常
    try_lock：
        尝试加锁，成功则 owns = true，失败则 owns = false 即 owns = pmutex->try_lock();
    unlock：
        释放当前持有的锁；pmutex->unlock(); owns = false; 若未持有锁，会抛异常
    release：
        放弃所有权但不解锁，返回 pmutex，并将自身置为空
        owns = false; auto __ret = pmutex; pmutex = nullptr; return __ret;
    mutex：
        返回当前关联的 mutex 指针
    owns_lock：
        返回当前是否持有锁 即 return owns;
    operator bool：
        等价于 owns_lock()

shared_lock  实现可移动的共享互斥量所有权包装器 (类模板)
    类似于 unique_lock 只是lock相关的操作都变为 shared

defer_lock_t、try_to_lock_t、adopt_lock_t 是三个标签类
defer_lock、try_to_lock、adopt_lock 是三个标签类的 constexpr 对象，用于指定锁定策略的标签

辅助函数：
    template< class Lockable1, class Lockable2, class... LockableN >
    int try_lock ( Lockable1& lock1, Lockable2& lock2, LockableN&... lockn );
        尝试通过按顺序调用每个给定 Lockable 对象 lock1、lock2、...、lockn 的 try_lock 来锁定它们，从第一个开始。
        如果对 try_lock 的调用失败，则不会执行对 try_lock 的后续调用，将对任何已锁定的对象调用 unlock，并返回失败锁定对象的从 ​0​ 开始的索引。
        如果对 try_lock 的调用导致异常，则在重新抛出之前，会对任何已锁定的对象调用 unlock。
        成功时返回 -1，或返回失败锁定对象的从 ​0​ 开始的索引值。

    template< class Lockable1, class Lockable2, class... LockableN >
    void lock( Lockable1& lock1, Lockable2& lock2, LockableN&... lockn );
        使用死锁避免算法锁定给定的 Lockable 对象 lock1、lock2、...、lockn 以避免死锁。
        这些对象通过一系列未指定的 lock、try_lock 和 unlock 调用来锁定。如果对 lock 或 unlock 的调用导致异常，则在重新抛出之前，会对所有已锁定的对象调用 unlock。

    template< class Callable, class... Args >
    void call_once( std::once_flag& flag, Callable&& f, Args&&... args );
        保证某段代码，在多个线程竞争时也只执行一次。配套的 std::once_flag 记录“是否已经执行过”的标记对象。
        可以理解成：once_flag = 状态位， call_once = 带线程安全的一次执行器
        如果，当调用std::call_once时，flag指示f已被调用过，std::call_once会立即返回。否则，std::call_once会调用f(args)。
        如果该调用抛出异常，则该异常会传播到std::call_once的调用者，并且flag不会被翻转，以便再次尝试调用。如果该调用正常返回，则flag会被翻转。
        与std::thread构造函数或std::async不同，参数不会被移动或复制，因为它们不需要转移到另一个执行线程。
*/

void foo_lock_guard(int &a, mutex &mtx) {
	lock_guard<mutex> lg(mtx);
	for(int i = 0; i < 1000000; ++i) {
		a += 1;
	}
}

void foo_bad(int &a, mutex &mtx) {
	lock_guard<mutex> {mtx};
	// ignoring return value of 'std::lock_guard<_Mutex>::lock_guard(mutex_type&) [with _Mutex = std::mutex; mutex_type = std::mutex]', declared with attribute 'nodiscard' [-Wunused-result]gcc
	// Wring 的意思是 这个对象不该被忽略。因为：忽略几乎一定写错了。
	// 注意：C++中临时对象（未命名的变量）的生命周期截止到完整表达式结束，因此后面都是无锁运行
	/*  这里写lock_guard<mutex> (mtx); 会直接无法通过编译，因为declaration parsing preference
        也就是：先尝试按声明解析，如果能成立：就当声明。不是表达式。
        lock_guard(mtx); 被看成：声明一个 lock_guard 类型变量，名字叫 mtx 
        括号被看作声明语法里的括号。不是构造参数。
        解决方法是用{}替换()*/
	for(int i = 0; i < 1000000; ++i) {
		a += 1;
	}
}

void foo_scoped_lock(int i, mutex &mtx1, mutex &mtx2) {
	scoped_lock sl(mtx1, mtx2);
	// 或者 lock(mtx1, mtx2); 加 mtx1.unlock(), mtx2.unlock();
	cout << i << " ";
}

atomic<bool> deadlock = false;
atomic<int> finished = 0;
void foo_two_bad(int i, mutex &m1, mutex &m2) {
	if(deadlock) {
		++finished;
		return;
	}
	m1.lock();
	m2.lock();
	if(!deadlock) {
		++finished;
		cout << i << " ";
	}
	m2.unlock();
	m1.unlock();
}

int main() {
	cout << boolalpha;
	{
		mutex mtx;
		int a = 0;
		std::thread t1(foo_lock_guard, std::ref(a), std::ref(mtx));
		std::thread t2(foo_lock_guard, std::ref(a), std::ref(mtx));
		t1.join();
		t2.join();
		cout << "using lock_guard, a = " << a << endl;
	} // 使用lock_guard自动上锁与解锁

	{
		mutex mtx;
		int a = 0;
		std::thread t1(foo_bad, std::ref(a), std::ref(mtx));
		std::thread t2(foo_bad, std::ref(a), std::ref(mtx));
		t1.join();
		t2.join();
		cout << "woring using lock_guard, a = " << a << endl;
	} // 注意：C++中临时对象（未命名的变量）的生命周期截止到完整表达式结束，因此实际上是无锁运行

	{
		mutex mtx1, mtx2;
		vector<thread> tv;
		for(int i = 0; i < 100; ++i) {
			tv.emplace_back(foo_scoped_lock, 2 * i, std::ref(mtx1), std::ref(mtx2));
			tv.emplace_back(foo_scoped_lock, 2 * i + 1, std::ref(mtx2), std::ref(mtx1));
		}
		for(auto &t : tv) t.join();
		cout << '\n' << "scoped_lock right used" << endl;
	} // 多个互斥锁用 scoped_lock 或者 void lock( Lockable1& , Lockable2&...); 防止死锁

	{
		mutex mtx1, mtx2;
		vector<thread> tv;
		for(int i = 0; i < 100; ++i) {
			tv.emplace_back(foo_two_bad, 2 * i, std::ref(mtx1), std::ref(mtx2));
			tv.emplace_back(foo_two_bad, 2 * i + 1, std::ref(mtx2), std::ref(mtx1));
		}
		int last = -1;
		while(finished < 200 and !deadlock) {
			this_thread::sleep_for(200ms);
			if(finished == last) {
				cout << "\nDeadlock detected\n";
				deadlock = true;
				mtx1.try_lock(), mtx2.try_lock();
				mtx1.unlock(), mtx2.unlock();
				break;
			}
			last = finished;
		}
		for(auto &t : tv) t.detach();
		cout << "woring using" << endl;
	} // 由于获取两个锁的次序不同，因此有可能会发生死锁

	{
		mutex mtx;
		{
			unique_lock<mutex> ul(mtx);
			cout << "默认构造: " << (mtx.try_lock() ? "mtx未上锁" : "mtx已经上锁") << "; "
			     << (ul.owns_lock() ? "ul拥有所有权" : "ul没有所有权") << endl; // 默认构造立刻加锁，并且拥有所有权
		}

		{
			unique_lock<mutex> ul(mtx, defer_lock);
			cout << "defer_lock: " << (mtx.try_lock() ? "mtx未上锁" : "mtx已经上锁") << "; "
			     << (ul.owns_lock() ? "ul拥有所有权" : "ul没有所有权") << endl; // 默认构造立刻加锁，并且拥有所有权
			mtx.unlock();

			ul.lock();
			cout << "  加锁之后: " << (mtx.try_lock() ? "mtx未上锁" : "mtx已经上锁") << "; "
			     << (ul.owns_lock() ? "ul拥有所有权" : "ul没有所有权") << endl; // 默认构造立刻加锁，并且拥有所有权
			cout << &mtx << " " << ul.mutex() << " " << ul.release() << " " << ul.mutex() << endl;
			<< " " << ul.owns_lock() << endl;
			cout << mtx.try_lock() << endl; // release只释放所有权，不解锁，因此返回false
		}

		{
			unique_lock<mutex> ul(mtx, adopt_lock);
			cout << ul.owns_lock() << endl;
			mtx.unlock();
			cout << boolalpha << ul.owns_lock() << endl;
		}
	}
}