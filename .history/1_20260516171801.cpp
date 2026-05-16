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
		int b = 0;
		std::thread(foo2, std::ref(b));
		.join(); // 使用 std::thread 和 std::ref
		std::thread(foo2, std::ref(b));
		.join();
		cout << "Final value of b: " << b << endl;
	}
	vector<int> vec {0, 0, 0};
	std::thread t1(foo3, std::ref(vec), 0);
	std::thread t2(foo3, std::ref(vec), 1);
	std::thread t3(foo3, std::ref(vec), 2);
	t1.join();
	t2.join();
	t3.join();
	cout << '[' << vec[0] << ' ' << vec[1] << ' ' << vec[2] << ']' << endl;
	return 0;
	return 0;
}
// 多个线程写入同一个内存区域需要上锁