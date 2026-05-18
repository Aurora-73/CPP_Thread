#include <chrono>
#include <format>
#include <iostream>
#include <memory> // 引入智能指针
#include <mutex>
#include <thread>
#include <vector>

// call_once 仅调用一次，单例 Log
/*
std::call_once 作用：

    template< class Callable, class... Args >
    void call_once(std::once_flag& flag, Callable&& f, Args&&... args);

1. 保证某段代码在多线程环境下只执行一次。
2. 配套的 std::once_flag 记录是否已经执行过。
3. 调用机制：
   - 第一次线程调用时，f(args...) 会立即在该线程执行。
   - 后续线程调用时，直接返回，不会再执行 f。
4. 异常处理：
   - 如果 f 抛出异常，once_flag 不会被标记为已调用。
   - 下次调用仍会尝试执行 f。
5. 参数不会被移动或复制：
   - 因为 call_once 在当前线程立即执行 f。
   - 与 std::thread 不同，std::thread 需要把参数转移到新线程，因此会复制/移动。
6. 使用场景：
   - 单例初始化
   - 全局资源初始化
   - 任何只需执行一次且需线程安全的操作
*/

using namespace std;

class Log {
public:
	static void init() {
		log.reset(new Log);
	}

	Log(const Log &log) = delete;
	Log &operator=(const Log &log) = delete;

	static Log &GetInstance() {
		static std::once_flag once;
		std::call_once(once, init); // 仅构造一个Log对象
		return *log;
	}

	void PrintLog(string msg) {
		std::lock_guard<std::mutex> lock(mtx); // 使用 lock_guard 自动管理锁
		cout << "[Log] " << msg << endl;
	}

private:
	Log() { };
	static std::unique_ptr<Log> log; // 使用智能指针
	// 类定义里面只是声明，而不是真正分配内存或者初始化。
	static std::mutex mtx;
}; // 使用call_once实现单例

// 定义静态成员变量
std::unique_ptr<Log> Log::log = nullptr; // C++只允许你声明静态成员，不能在类里直接分配空间或初始化
std::mutex Log::mtx;

class log2 {
public:
	static log2 &GetInstance() {
		static log2 instance;
		return instance;
	}

	void Printlog(const string &msg) {
		lock_guard<mutex> lock(mtx);
		cout << "[log2] " << msg << endl;
	}

private:
	log2() { }
	log2(const log2 &) = delete;
	log2 &operator=(const log2 &) = delete;
	mutex mtx;
}; // 更简单的写法是使用static局部静态变量，线程安全由 C++11 局部静态变量保证。

void print_test(int i) {
	Log::GetInstance().PrintLog(std::format("test_{}", i));
}

int main() {
	const int n = 100;
	vector<thread> vt;

	for(int i = 0; i < n; ++i) {
		vt.emplace_back(print_test, i);
	}

	for(auto &t : vt) {
		t.join();
	}

	return 0;
}