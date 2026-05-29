#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// call_once 仅调用一次，单例 Log
/*
template< class Callable, class... Args >
void call_once(std::once_flag& flag, Callable&& f, Args&&... args);

	1. 保证某段代码在多线程环境下只执行一次。
	2. 配套的 std::once_flag 记录是否已经执行过。
	3. 调用机制：
		第一次线程调用时，f(args...) 会立即在该线程执行。
		后续线程调用时，直接返回，不会再执行 f。
	4. 异常处理：
		如果 f 抛出异常，once_flag 不会被标记为已调用。
		下次调用仍会尝试执行 f。
	5. 参数不会被移动或复制：
		因为 call_once 在当前线程立即执行 f。
		与 std::thread 不同，std::thread 需要把参数转移到新线程，因此会复制/移动。
	6. 使用场景：
		单例初始化
		全局资源初始化
		任何只需执行一次且需线程安全的操作
*/

using namespace std;

class Log {
public:
	static void init() {
		log.reset(new Log);
	}

	Log(const Log &) = delete;
	Log &operator=(const Log &) = delete;

	static Log &GetInstance() {
		static std::once_flag once;
		std::call_once(once, init); // 仅构造一个Log对象
		return *log;
	}

	void PrintLog(const string &msg) {
		std::lock_guard<std::mutex> lock(mtx);
		cout << "[Log] " << msg << endl;
	}

private:
	Log() { }
	static std::unique_ptr<Log> log;
	std::mutex mtx; // 因为是单例，所以 mutex 只有一个也可以
}; // 使用call_once实现单例

// 定义静态成员变量
std::unique_ptr<Log> Log::log = nullptr;

class Log2 {
public:
	static Log2 &GetInstance() {
		static Log2 instance;
		return instance;
	}

	void PrintLog(const string &msg) {
		lock_guard<mutex> lock(mtx);
		cout << "[Log2] " << msg << endl;
	}

private:
	Log2() { }
	Log2(const Log2 &) = delete;
	Log2 &operator=(const Log2 &) = delete;
	mutex mtx;
}; // 更简单的写法是使用static局部静态变量，线程安全由 C++11 局部静态变量保证。

void print_test(int i) {
	Log::GetInstance().PrintLog(std::format("test_{}", i));
}

void print_test2(int i) {
	Log2::GetInstance().PrintLog(std::format("test_{}", i));
}

int main() {
	const int n = 100;

	{
		vector<thread> vt;
		for(int i = 0; i < n; ++i) {
			vt.emplace_back(print_test, i);
		}
		for(auto &t : vt) {
			t.join();
		}
	}

	{
		vector<thread> vt;
		for(int i = 0; i < n; ++i) {
			vt.emplace_back(print_test2, i);
		}
		for(auto &t : vt) {
			t.join();
		}
	}

	return 0;
}