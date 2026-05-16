#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// 线程 与 信号量
using namespace std;
std::mutex mtx;

void foo1(int &a) {
	for(int i = 0; i < 1000000; ++i) {
		a += 1;
	}
}

void foo2(int &a) {
	for(int i = 0; i < 1000000; ++i) {
		mtx.lock();
		a += 1;
		mtx.unlock();
	}
}

void foo3(vector<int> &vec, int id) {
	for(int i = 0; i < 100000; ++i) {
		vec[id]++;
	}
}

int main() {
	{
		int a = 0;
		std::thread t1(foo1, std::ref(a)); // 使用 std::thread 和 std::ref
		std::thread t2(foo1, std::ref(a));
		t1.join();
		t2.join();
		cout << "Final value of a: " << a << endl;
	}

	{
		int a = 0;
		std::thread t1(foo2, std::ref(a)); // 使用 std::thread 和 std::ref
		std::thread t2(foo2, std::ref(a));
		t1.join();
		t2.join();
		cout << "Final value of a: " << a << endl;
	} // 多个线程写入同一个内存区域需要上锁

	{
		vector<int> vec {0, 0, 0};
		std::thread t1(foo3, std::ref(vec), 0);
		std::thread t2(foo3, std::ref(vec), 1);
		std::thread t3(foo3, std::ref(vec), 2);
		t1.join();
		t2.join();
		t3.join();
		cout << '[' << vec[0] << ' ' << vec[1] << ' ' << vec[2] << ']' << endl;
	} // 多个线程写入不同的内存区域不需要上锁
	this_thread::sleep_for(chrono::milliseconds(100));
	this_thread::sleep_for(100ms);
	return 0;
}
/*
std::thread 表示单个执行线程。线程允许多个函数并发执行
	构造函数：函数指针, 参数
		其中参数默认是拷贝，即使函数指针中的参数是&，需要用std::ref才能传递引用
		注意引用类型的参数的生命周期不能小于在线程中使用的周期
	支持移动语义，不支持拷贝
	joinable：检查线程是否可join，即是否可能在并行上下文中运行
	get_id：返回线程的 id
	join：等待线程完成其执行
	detach：允许线程独立于线程句柄执行

this_thread::sleep_for：休眠当前线程

std::mutex 同步原语，可用于保护共享数据免遭多个线程同时访问。
	lock：锁定互斥体，如果互斥体不可用则阻塞
	try_lock：尝试锁定互斥体，如果互斥体不可用不阻塞返回false，反之则锁定并返回true
	unlock：解锁互斥体

std::timed_mutex 提供带超时尝试获取 timed_mutex 所有权的能力。
	lock：锁定互斥体，如果互斥体不可用则阻塞
	try_lock：尝试锁定互斥体，如果互斥体不可用则返回
	try_lock_for：尝试锁定互斥锁，如果互斥锁在指定超时时间内不可用，则返回false
	try_lock_until：尝试锁定互斥锁，如果互斥锁在指定超时时间内不可用，则返回false
	unlock：解锁互斥体

std::recursive_mutex 提供独占的、递归的所有权语义。可以被【同一个线程】锁定多次，每次解锁只会使锁定次数-1，只有解除全部的锁才会解锁互斥体。如果在被某个线程锁定时被另一个线程锁定则会进入
*/