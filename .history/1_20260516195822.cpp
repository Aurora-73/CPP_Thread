#include <chrono>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

// 线程 与 mutex
/*
std::thread 表示一个执行线程，可用于并发执行函数。

    构造函数：函数对象 + 参数
        参数默认按值拷贝，即使函数参数是引用类型，也需要 std::ref 显式传引用
        引用参数所引用对象的生命周期必须覆盖线程使用周期

    支持移动，不支持拷贝

    joinable：检查当前 thread 对象是否关联线程执行对象
    get_id：返回线程 id
    join：等待线程执行结束
    detach：线程独立运行，thread 对象失去控制权

    注意：
        若 std::thread 析构时joinable() == true，会调用 std::terminate 抛出异常
        因此析构前必须 join 或 detach

this_thread::sleep_for： 当前线程休眠指定时长
this_thread::sleep_until： 当前线程休眠到指定时间点
this_thread::get_id： 返回当前线程 id
this_thread::yield： 向实现提供一个提示，以重新调度线程的执行，允许其他线程运行。
*/


/*
std::mutex 基础互斥量，同一时刻仅允许一个线程持有
    lock：加锁，若已被占用则阻塞
    try_lock：尝试加锁，若无法立即获得锁则返回 false
    unlock：解锁
    注意：同一线程重复 lock 普通 mutex 是未定义行为（通常死锁）


std::timed_mutex 支持超时加锁
    lock / try_lock / unlock 同上
    try_lock_for： 在指定时间内尝试获取锁，获取成功返回true，不成功返回false，不阻塞
    try_lock_until： 在指定时间点前尝试获取锁，获取成功返回true，不成功返回false，不阻塞


std::recursive_mutex 支持同一线程重复加锁
    同一线程可多次 lock，每次 unlock 释放一层持有，直到全部 unlock 才真正释放互斥量
    若其他线程请求锁，则阻塞
    注意：
        递归层数有实现限制，超过可能抛出异常


std::recursive_timed_mutex
    recursive_mutex + timed_mutex


std::shared_mutex 支持共享锁和独占锁

    独占锁：
        lock
        try_lock
        unlock
        持有独占锁时：
            其他线程共享锁和独占锁请求都阻塞

    共享锁：
        lock_shared
        try_lock_shared
        unlock_shared
        多个线程可同时持有共享锁
        存在共享锁时：
		    独占锁请求阻塞
*/

using namespace std;
using namespace std::chrono_literals;

mutex mtx;
timed_mutex timed_mtx;
recursive_mutex recursive_mtx;
shared_mutex rw_mtx;

constexpr int kUnsafeLoopCount = 1000000;
constexpr int kSafeLoopCount = 1000000;
constexpr int kVectorLoopCount = 100000;
constexpr int kSharedLoopCount = 3;

void foo1(int &a) {
	for(int i = 0; i < kUnsafeLoopCount; ++i) {
		a += 1;
	}
}

void foo2(int &a) {
	for(int i = 0; i < kSafeLoopCount; ++i) {
		mtx.lock();
		a += 1;
		mtx.unlock();
	}
}

void foo3(vector<int> &vec, int id) {
	for(int i = 0; i < kVectorLoopCount; ++i) {
		vec[id]++;
	}
}

void hold_timed_mutex() {
	timed_mtx.lock();
	cout << "timed_mutex: worker holds lock for 300ms" << endl;
	this_thread::sleep_for(300ms);
	timed_mtx.unlock();
}

void recursive_print(int depth) {
	if(depth == 0) {
		return;
	}
	recursive_mtx.lock();
	cout << "recursive_mutex depth = " << depth << endl;
	recursive_print(depth - 1);
	recursive_mtx.unlock();
}

void shared_reader(int id, int &value) {
	for(int i = 0; i < kSharedLoopCount; ++i) {
		rw_mtx.lock_shared();
		cout << "reader " << id << " reads value = " << value << endl;
		rw_mtx.unlock_shared();
		this_thread::sleep_for(50ms);
	}
}

void shared_writer(int &value) {
	for(int i = 0; i < kSharedLoopCount; ++i) {
		rw_mtx.lock();
		++value;
		cout << "writer updates value to " << value << endl;
		rw_mtx.unlock();
		this_thread::sleep_for(80ms);
	}
}

int main() {
	{
		int a = 0;
		thread t1(foo1, ref(a)); // 使用 thread 和 ref
		thread t2(foo1, ref(a));
		t1.join();
		t2.join();
		cout << "Unsafe final value of a: " << a << endl;
	} // 这里有数据竞争，结果是未定义行为，只用于演示“不加锁会出问题”

	{
		int a = 0;
		thread t1(foo2, ref(a));
		thread t2(foo2, ref(a));
		t1.join();
		t2.join();
		cout << "Safe final value of a: " << a << endl;
	} // 多个线程写入同一个内存区域需要上锁

	{
		vector<int> vec { 0, 0, 0 };
		thread t1(foo3, ref(vec), 0);
		thread t2(foo3, ref(vec), 1);
		thread t3(foo3, ref(vec), 2);
		t1.join();
		t2.join();
		t3.join();
		cout << '[' << vec[0] << ' ' << vec[1] << ' ' << vec[2] << ']' << endl;
	} // vector 已固定大小，且每个线程只写不同下标，因此这里不需要上锁

	{
		thread t1(hold_timed_mutex);
		this_thread::sleep_for(50ms);
		if(timed_mtx.try_lock_for(100ms)) {
			cout << "timed_mutex: main got lock within 100ms" << endl;
			timed_mtx.unlock();
		} else {
			cout << "timed_mutex: main failed within 100ms" << endl;
		}
		if(timed_mtx.try_lock_for(400ms)) {
			cout << "timed_mutex: main got lock within 400ms" << endl;
			timed_mtx.unlock();
		}
		t1.join();
	} // 超时锁，尝试在一段时间内获取锁，获取成功返回true，失败返回false，不阻塞

	{
		recursive_print(3);
	} // 递归锁，可以被同一个线程多次锁定，每次锁定计数器+1，直到计数器清0才释放锁

	{
		int value = 0;
		thread writer(shared_writer, ref(value));
		thread reader1(shared_reader, 1, ref(value));
		thread reader2(shared_reader, 2, ref(value));
		writer.join();
		reader1.join();
		reader2.join();
	} // 读写锁，施加读锁后尝试获取写锁会阻塞，施加写锁后尝试获取读写锁都会阻塞

	{
		cout << "main thread sleep for 100ms" << endl;
		this_thread::sleep_for(chrono::milliseconds(100));
		cout << "main thread sleep for another 100ms" << endl;
		this_thread::sleep_for(100ms);
	} // 阻塞
	return 0;
}