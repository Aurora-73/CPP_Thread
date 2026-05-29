#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <future>
#include <iostream>
#include <limits>
#include <mutex>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

/* 固定任务签名的专用线程池：
1. 队列中直接保存 packaged_task<R()>
2. 每个线程池实例只处理一种返回值类型的任务
3. 提交的任务本身就需要是擦除参数类型的
4. submit 创建packaged_task，返回值包装为 future */

using namespace std;

template <typename R>
class FixedThreadPool {
public:
	using result_type = R;
	using task_type = packaged_task<R()>;

	FixedThreadPool(size_t worker_count, size_t capacity) : max_size(capacity) {
		if(worker_count == 0) {
			throw invalid_argument("worker count must be positive");
		}
		if(capacity == 0) {
			throw invalid_argument("task queue capacity must be positive");
		}

		workers.reserve(worker_count);
		for(size_t i = 0; i < worker_count; ++i) {
			workers.emplace_back(&FixedThreadPool::worker_loop, this);
		}
	}

	FixedThreadPool() :
	    FixedThreadPool(max(1u, std::thread::hardware_concurrency()),
	                    max(1u, std::thread::hardware_concurrency()) * 2) { }

	~FixedThreadPool() {
		shutdown();
	}

	FixedThreadPool(const FixedThreadPool &) = delete;
	FixedThreadPool &operator=(const FixedThreadPool &) = delete;

	future<result_type> submit(task_type task) {
		future<result_type> fu = task.get_future();

		{
			unique_lock<mutex> lk(mtx);
			// 有界队列：满了就等待，关闭后停止接收新任务。
			cv_not_full.wait(lk, [this] { return tasks.size() < max_size || stop_flag; });

			if(stop_flag) {
				throw runtime_error("thread pool has been stopped");
			}

			tasks.emplace(std::move(task));
		}

		cv_not_empty.notify_one();
		return fu;
	}

	void shutdown() {
		if(stop_flag.exchange(true)) {
			return;
		}

		// 唤醒所有可能阻塞在“队列满/空”条件上的线程，让它们观察到停止状态。
		cv_not_full.notify_all();
		cv_not_empty.notify_all();

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
		cv_not_full.notify_all();
		cv_not_empty.notify_all();
		for(auto &worker : workers) {
			if(worker.joinable()) {
				worker.join();
			}
		}
		return remaining_tasks;
	}

private:
	void worker_loop() {
		while(true) {
			task_type task;
			{
				unique_lock<mutex> lk(mtx);
				// 队列为空时等待；关闭后若队列也空了，worker 才真正退出。
				cv_not_empty.wait(lk, [this] { return !tasks.empty() || stop_flag; });

				if(tasks.empty()) {
					break;
				}

				task = std::move(tasks.front());
				tasks.pop();
			}

			cv_not_full.notify_one();
			task();
		}
	}

private:
	const size_t max_size;
	queue<task_type> tasks;
	vector<thread> workers;
	mutex mtx;
	condition_variable cv_not_full;
	condition_variable cv_not_empty;
	atomic<bool> stop_flag = false;
};

string longestPalindrome(const string &);

int main() {
	constexpr int task_count = 10000;

	FixedThreadPool<string> thread_pool;

	vector<future<string>> result_futures;
	result_futures.reserve(task_count);

	mt19937 rng(random_device {}());
	uniform_int_distribution<int> dist(0, numeric_limits<int>::max());

	for(int i = 0; i < task_count; ++i) {
		string input = format("{}{}{}{}", dist(rng), dist(rng), dist(rng), dist(rng));
		packaged_task<string()> task([input = std::move(input)] { return longestPalindrome(input); });
		result_futures.push_back(thread_pool.submit(std::move(task)));
	}

	cout << "\n[main] 所有任务已提交，开始统一收集结果\n" << endl;

	for(int i = 0; i < task_count; ++i) {
		cout << format("[result] task {} -> {}", i, result_futures[i].get()) << endl;
	}

	thread_pool.shutdown(); // 析构函数已经会调用 shutdown()，这里显式调用是多余的。

	return 0;
}

string longestPalindrome(const string &s) {
	cout << format("计算 \"{}\" 的最长回文子串", s) << endl;

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