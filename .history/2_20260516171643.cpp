#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// 访问不同内存可以不上锁

using namespace std;
mutex mtx;

int main() {
	vector<int> vec {0, 0, 0};
	std::thread t1(foo, std::ref(vec), 0);
	std::thread t2(foo, std::ref(vec), 1);
	std::thread t3(foo, std::ref(vec), 2);
	t1.join();
	t2.join();
	t3.join();
	cout << '[' << vec[0] << ' ' << vec[1] << ' ' << vec[2] << ']' << endl;
	return 0;
}
// t1 t2 t3分别访问不同的内存，不需要上锁
// 但是如果多一个进程修改vector的长度，则需要全部上锁