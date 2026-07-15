#include <atomic>
#include <condition_variable>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
	#define THREAD_POOL_HAS_MOVE_ONLY_FUNCTION 1
#else
	#define THREAD_POOL_HAS_MOVE_ONLY_FUNCTION 0
#endif

/* 泛型线程池：
1. submit 接收任意可调用对象和参数
2. 队列中统一保存“无参任务”，worker 只负责不断取出并执行
3. C++23 下优先使用 move_only_function<void()>，否则退化为 function<void()> + shared_ptr */

using namespace std;

class ThreadPool {
public:
	ThreadPool(unsigned int num_threads) {
		if(num_threads == 0) {
			throw invalid_argument("thread count must be positive");
		}
		for(unsigned int i = 0; i < num_threads; ++i) {
			workers.emplace_back(&ThreadPool::worker_loop, this);
		}
	}

	ThreadPool() : ThreadPool(max(1u, thread::hardware_concurrency())) { }

	~ThreadPool() {
		shutdown();
	}

	ThreadPool(const ThreadPool &) = delete;
	ThreadPool &operator=(const ThreadPool &) = delete;
	ThreadPool(ThreadPool &&) = delete;            // mutex、condition_variable、atomic 都不支持移动语义
	ThreadPool &operator=(ThreadPool &&) = delete; // 即使不显式删除也会自动删除移动语义

	template <typename F, typename... Args>
	future<std::invoke_result_t<F, Args...>> submit(F &&fun, Args &&...args) {
		using result_type = std::invoke_result_t<F, Args...>;

		if(stop_flag) throw runtime_error("ThreadPool has been stopped");

#if THREAD_POOL_HAS_MOVE_ONLY_FUNCTION
		// std::move_only_function 支持只移动不拷贝的任务包装，因此无需 shared_ptr。
		packaged_task<result_type(Args...)> pt(std::forward<F>(fun));
		future<result_type> fu = pt.get_future();
		// 先把参数打包保存为 tuple 捕获到 lamda 表达式中，再在 worker 线程中使用 apply 作用为参数，将任务统一擦除成 void()。
		auto bound_task = [pt = std::move(pt), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
			// std::ref 是因为 std::apply 会拷贝可调用对象，而 packaged_task 是 move-only 的，所以用 std::ref 绕过拷贝
			std::apply(std::ref(pt), std::move(tup));
		};
#else
		// std::function 需要可拷贝任务，因此用 shared_ptr 托管 packaged_task。
		auto pt = std::make_shared<std::packaged_task<result_type(Args...)>>(std::forward<F>(fun));
		future<result_type> fu = pt->get_future();
		auto bound_task = [pt, tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
			std::apply(std::ref(*pt), std::move(tup));
		};
#endif

		{
			lock_guard<mutex> lk(mtx);
			if(stop_flag) throw runtime_error("ThreadPool has been stopped");
			tasks.emplace(std::move(bound_task));
		}

		cv.notify_one();
		return fu;
	}

	void shutdown() {
		if(stop_flag.exchange(true)) {
			return;
		}
		cv.notify_all();
		for(auto &worker : workers) {
			if(worker.joinable()) {
				worker.join();
			}
		}
	}

	auto shutdown_now() {
		if(stop_flag.exchange(true)) {
			return decltype(tasks) {};
		}
		decltype(tasks) remaining_tasks;
		{
			lock_guard<mutex> lk(mtx);
			// 立即停止时，把尚未被 worker 取走的任务整体移交给调用者。
			swap(remaining_tasks, tasks);
		}
		cv.notify_all();
		for(auto &worker : workers) {
			if(worker.joinable()) {
				worker.join();
			}
		}
		return remaining_tasks; // NRVO
	}

private:
	mutex mtx;
	atomic<bool> stop_flag = false;
	condition_variable cv;
	vector<thread> workers;
#if THREAD_POOL_HAS_MOVE_ONLY_FUNCTION
	queue<move_only_function<void()>> tasks;
#else
	queue<function<void()>> tasks;
#endif

	void worker_loop() {
		while(true) {
			unique_lock<mutex> lk(mtx);
			// 关闭后若队列也空了，worker 才退出；否则继续把剩余任务做完。
			cv.wait(lk, [&]() { return stop_flag || !tasks.empty(); });
			if(stop_flag && tasks.empty()) {
				break;
			}
			auto task = std::move(tasks.front());
			tasks.pop();
			lk.unlock();
			task(); // 并行执行任务，packaged_task 内部已自动捕获异常存入 future
		}
	}
};

string task1(string &&);

int main() {
	ThreadPool thread_pool;

	auto task2 = [](string &&s) -> string {
		this_thread::sleep_for(1ms);
		return s + " finished";
	};

	mt19937 rng(random_device {}());
	uniform_int_distribution<int> dist(0, numeric_limits<int>::max());

	vector<future<string>> result_futures1;
	for(int i = 0; i < 100; ++i) {
		result_futures1.emplace_back(thread_pool.submit(task1, format("{}{}{}", dist(rng), dist(rng), dist(rng))));
	}

	vector<future<string>> result_futures2;
	for(int i = 0; i < 100; ++i) {
		result_futures2.emplace_back(thread_pool.submit(task2, format("task_{}", i)));
	}

	thread_pool.shutdown();

	std::cout << "ThreadPool stopped" << endl;

	for(auto &fu : result_futures1) {
		std::cout << fu.get() << endl;
	}

	for(auto &fu : result_futures2) {
		std::cout << fu.get() << endl;
	}

	return 0;
}

string task1(string &&s) {
	// 最长回文子串作为示例
	int n = static_cast<int>(s.size());
	if(n == 0) return "";

	int res_beg = 0, res_size = 1;

	for(int i = 1; i < n - (res_size - 1) / 2; ++i) {
		int len = 0;
		while(i - len - 1 >= 0 && i + len + 1 < n && s[i - len - 1] == s[i + len + 1]) ++len;
		if(len * 2 + 1 > res_size) {
			res_beg = i - len;
			res_size = len * 2 + 1;
		}
	}

	for(int i = (res_size + 1) / 2; i < n - res_size / 2; ++i) {
		if(s[i] != s[i - 1]) continue;
		int len = 0;
		while(i - len - 2 >= 0 && i + len + 1 < n && s[i - len - 2] == s[i + len + 1]) ++len;
		if(len * 2 + 2 > res_size) {
			res_beg = i - len - 1;
			res_size = len * 2 + 2;
		}
	}

	return s.substr(res_beg, res_size);
}
