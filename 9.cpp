#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

// 使用 packaged_task 实现任务队列：
// 1. 提交方创建任务并拿到 future
// 2. 工作线程从队列取出任务并执行
// 3. 提交方稍后通过 future 获取结果

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

class TaskQueue {
private:
	const size_t max_size;
	queue<packaged_task<string()>> tasks;
	mutex mtx;
	condition_variable cv_not_full;
	condition_variable cv_not_empty;
	bool stop = false;

public:
	explicit TaskQueue(size_t cap) : max_size(cap) { }

	future<string> submit(string input) {
		packaged_task<string()> task([input = std::move(input)] {
			this_thread::sleep_for(milliseconds(10)); // 模拟处理开销
			return longestPalindrome(input);
		});

		future<string> fu = task.get_future();

		{
			unique_lock<mutex> lk(mtx);
			cv_not_full.wait(lk, [this] { return tasks.size() < max_size || stop; });

			if(stop) {
				throw runtime_error("task queue has been stopped");
			}

			tasks.push(std::move(task));
		}

		cv_not_empty.notify_one();
		return fu;
	}

	bool take(packaged_task<string()> &task) {
		unique_lock<mutex> lk(mtx);
		cv_not_empty.wait(lk, [this] { return !tasks.empty() || stop; });

		if(tasks.empty()) {
			return false; // stop == true 且队列已空
		}

		task = std::move(tasks.front());
		tasks.pop();

		lk.unlock();
		cv_not_full.notify_one();
		return true;
	}

	void shutdown() {
		{
			lock_guard<mutex> lk(mtx);
			stop = true;
		}
		cv_not_full.notify_all();
		cv_not_empty.notify_all();
	}
};

void worker(TaskQueue &task_queue, int worker_id) {
	while(true) {
		packaged_task<string()> task;
		if(!task_queue.take(task)) {
			break;
		}

		cout << format("[worker] {} 执行任务", worker_id) << endl;
		task(); // 这里执行后，对应 future 就会变为 ready
	}

	cout << format("[worker] {} exit", worker_id) << endl;
}

string make_input(int i, int id) {
	thread_local mt19937 eg(random_device {}());
	uniform_int_distribution<int> uf(0, INT_MAX);

	auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	return format("{}{}{}{}", i, now, id, uf(eg));
}

int main() {
	constexpr int worker_count = 3;
	constexpr int task_count = 12;

	TaskQueue task_queue(5);
	vector<thread> workers;
	workers.reserve(worker_count);

	for(int i = 0; i < worker_count; ++i) {
		workers.emplace_back(worker, std::ref(task_queue), i);
	}

	vector<future<string>> results;
	results.reserve(task_count);

	for(int i = 0; i < task_count; ++i) {
		string input = make_input(i % 3, i);
		cout << format("[main] 提交任务 {}", input) << endl;
		results.push_back(task_queue.submit(std::move(input)));
	}

	cout << "\n[main] 所有任务已提交，开始统一收集结果\n" << endl;

	for(int i = 0; i < task_count; ++i) {
		cout << format("[result] task {} -> {}", i, results[i].get()) << endl;
	}

	task_queue.shutdown();

	for(int i = 0; i < worker_count; ++i) {
		workers[i].join();
		cout << format("[main] worker {} joined", i) << endl;
	}

	return 0;
}