#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

// RAII 管理锁

using namespace std;
std::mutex mtx;
std::timed_mutex tmtx;

void foo1(int &a) {
	for(int i = 0; i < 100000; ++i) {
		std::lock_guard<std::mutex> lg(mtx);
		a += 1;
	}
}

void foo2(int &a) {
	for(int i = 0; i < 10; ++i) {
		std::unique_lock<std::timed_mutex> lg(tmtx, std::defer_lock);
		if(lg.try_lock_for(std::chrono::nanoseconds(2))) {
			std::this_thread::sleep_for(std::chrono::nanoseconds(4));
			a += 1;
		} else {
			std::cout << "放弃" << endl;
		} // 尝试在2纳秒内获取锁，获取不到就放弃
	}
}

int main() {
	int a = 0;
	std::thread t1(foo1, std::ref(a)); // 使用 std::thread 和 std::ref
	std::thread t2(foo1, std::ref(a));
	t1.join();
	t2.join();
	cout << "Final value of a: " << a << endl; // Output: 2000000
	a = 0;
	std::thread t3(foo2, std::ref(a)); // 使用 std::thread 和 std::ref
	std::thread t4(foo2, std::ref(a));
	t3.join();
	t4.join();
	cout << "Final value of a: " << a << endl; // Output: 2000000
	return 0;
}