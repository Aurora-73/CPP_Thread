#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

// 线程 与 信号量

using namespace std;
std::mutex mtx;

void foo(int &a) {
	for(int i = 0; i < 1000000; ++i) {
		mtx.lock();
		a += 1;
		mtx.unlock();
	}
}

int main() {
	int a = 0;
	std::thread t1(foo, std::ref(a)); // 使用 std::thread 和 std::ref
	std::thread t2(foo, std::ref(a));
	t1.join();
	t2.join();
	cout << "Final value of a: " << a << endl; // Output: 2000000
	return 0;
}