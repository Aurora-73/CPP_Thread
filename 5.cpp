#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <memory> // 引入智能指针

// call_once 仅调用一次，单例 Log

using namespace std;

class Log {
public:
	static void init() {
		if(!log)
			log = std::unique_ptr<Log>(new Log);
	}

	Log(const Log &log) = delete;
	Log &operator=(const Log &log) = delete;

	static Log &GetInstance() {
		std::call_once(once, init); // 仅构造一个Log对象
		return *log;
	}

	void PrintLog(string msg) {
		std::lock_guard<std::mutex> lock(mtx); // 使用 lock_guard 自动管理锁
		cout << "[Log] " << msg << endl;
	}

private:
	Log() {};
	static std::unique_ptr<Log> log; // 使用智能指针
	// 类定义里面只是声明，而不是真正分配内存或者初始化。
	static std::once_flag once;
	static std::mutex mtx;
};

// 定义静态成员变量
std::unique_ptr<Log> Log::log = nullptr; // C++只允许你声明静态成员，不能在类里直接分配空间或初始化
std::once_flag Log::once;
std::mutex Log::mtx;

void print_test() {
	Log::GetInstance().PrintLog("test");
}

int main() {
	const int n = 100;
	vector<thread> threads;

	for(int i = 0; i < n; ++i) {
		threads.emplace_back(print_test);
	}

	for(auto &t : threads) {
		t.join();
	}

	return 0;
}