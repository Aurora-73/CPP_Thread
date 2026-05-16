#include <chrono>
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

int main() {
	int a = 0;
	std::thread t1(foo1, std::ref(a)); // 使用 std::thread 和 std::ref
	std::thread t2(foo1, std::ref(a));
	t1.join();
	t2.join();
	cout << "Final value of a: " << a << endl;

	int b = 0;
	std::thread t3(foo2, std::ref(b)); // 使用 std::thread 和 std::ref
	std::thread t4(foo2, std::ref(b));
	t3.join();
	t4.join();
	cout << "Final value of b: " << b << endl;
	return 0;
}