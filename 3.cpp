#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

// 死锁

using namespace std;
mutex m1, m2;

void foo1() {
	for(int i = 0; i < 100000; ++i) {
		m1.lock();
		m2.lock();
		m2.unlock();
		m1.unlock();
	}
}

void foo2() {
	for(int i = 0; i < 100000; ++i) {
		m2.lock();
		m1.lock();
		m1.unlock();
		m2.unlock();
	}
}

int main() {
	std::thread t1(foo1);
	std::thread t2(foo2);
	t1.join();
	t2.join();
	return 0;
}