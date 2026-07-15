#include <atomic>
#include <chrono>
#include <condition_variable>
#include <execution>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <tuple>
#include <vector>

using namespace std;

// 任务窃取线程池
/*  Work-Stealing 思路

  核心区别：你的线程池是全局共享队列，所有 worker 竞争同一把锁。Work-stealing 是每线程一个本地队列：

  旧的方案：  所有任务 → [全局队列] ← 所有 worker 竞争
  Stealing：  任务 round-robin → [队列0] [队列1] [队列2] ...
                                 ↑         ↑         ↑
                               worker0   worker1   worker2
              空闲时从别人队列尾部偷任务

  优势：高并发下锁竞争大幅减少（本地操作无竞争），适合递归并行（如并行快排/并行 for_each），这也是 Intel TBB、Go runtime、Folly 线程池的默认策略。

  实现上就多两件事：
  1. 把 queue<function<void()>> 换成 vector<queue> 每线程一个
  2. worker 空闲时遍历其他队列 try_steal

  1. 先理解 work-stealing 思路 — 用一个加锁的 std::deque 就能实现，重点是理解"本地 LIFO + 远程 FIFO"的策略，面试能讲清楚就够
  2. 环形无锁队列 — 更实用（Disruptor 模式），实现相对简单，面试手撕可行性高
  3. 链表无锁队列（Michael-Scott） — 最难，需要配合 Hazard Pointer，适合作为深度加分项

  不需要为了写 work-stealing 线程池而去实现无锁队列。一个 std::deque<std::function<void()>> + std::mutex 就够了：

  class WorkStealingPool {
      struct WorkerQueue {
          std::mutex mtx;
          std::deque<std::function<void()>> tasks;
      };
      std::vector<WorkerQueue> queues;  // 每线程一个
      // owner: lock 自己的 queue，push_back / pop_back
      // thief:  lock 别人的 queue，pop_front
  };

  这才是 work-stealing 的最小实现——关键在架构（每线程本地队列 + 偷取策略），不在队列的无锁化。
*/

using namespace std;

class ThreadPool {
public:
#if __cplusplus > 202002L && _GLIBCXX_HOSTED
	using function_t = move_only_function<void()>;
#else
	using function_t = function<void()>;
#endif

	explicit ThreadPool(unsigned n) : size(n), local_tasks(n), local_mtx(n) {
		threads.reserve(n);
		for(unsigned i = 0; i < n; ++i) {
			threads.emplace_back(&ThreadPool::work, this, i);
		}
	}

	ThreadPool() : ThreadPool(max(thread::hardware_concurrency(), 1u)) { }

	ThreadPool(ThreadPool &&) = delete;
	ThreadPool(const ThreadPool &) = delete;

	template <typename F, typename... Args>
	auto submit(F &&f, Args &&...args) {
		using return_t = invoke_result_t<F, Args...>;

		if(done) throw runtime_error("ThreadPool has been stopped");

		// worker 线程提交到自己的本地队列（LIFO），外部线程 round-robin
		unsigned id;
		if(tl_my_id != -1) {
			id = static_cast<unsigned>(tl_my_id);
		} else {
			id = curr_id.fetch_add(1) % size;
		}

#if __cplusplus > 202002L && _GLIBCXX_HOSTED
		packaged_task<return_t(Args...)> pt(forward<F>(f));
		future<return_t> res = pt.get_future();
		{
			lock_guard<mutex> lk(local_mtx[id]);
			local_tasks[id].emplace_back(
			    [pt = move(pt), tup = make_tuple(forward<Args>(args)...)]() mutable { apply(ref(pt), move(tup)); });
		}
#else
		auto pt = make_shared<packaged_task<return_t(Args...)>>(forward<F>(f));
		future<return_t> res = pt->get_future();
		{
			lock_guard<mutex> lk(local_mtx[id]);
			local_tasks[id].emplace_back(
			    [pt, tup = make_tuple(forward<Args>(args)...)]() mutable { apply(*pt, move(tup)); });
		}
#endif

		cv.notify_one();
		return res;
	}

	bool is_exit() {
		return done.load();
	}

	void shutdown() {
		if(done.exchange(true)) return;
		cv.notify_all();
		for(auto &th : threads) th.join();
	}

	vector<function_t> shutdown_now() {
		vector<function_t> remaining;
		if(done.exchange(true)) return remaining;
		for(unsigned i = 0; i < size; ++i) {
			lock_guard<mutex> lk(local_mtx[i]);
			while(!local_tasks[i].empty()) {
				remaining.emplace_back(move(local_tasks[i].front()));
				local_tasks[i].pop_front();
			}
		}
		cv.notify_all();
		for(auto &th : threads) th.join();
		return remaining;
	}

private:
	const unsigned size;
	atomic<bool> done = false;
	atomic<unsigned> curr_id = 0;
	inline static thread_local int tl_my_id = -1;

	vector<deque<function_t>> local_tasks; // owner: back 进 back 出 (LIFO)
	vector<mutex> local_mtx;
	mutex global_mtx;
	condition_variable cv;
	vector<jthread> threads;

	// 偷取：从别人队列的 front 取（FIFO）
	function_t try_steal(unsigned id) {
		// 随机起点避免所有 thief 从同一个队列开始争抢
		static thread_local mt19937 rng(random_device {}());
		unsigned start = rng() % size;
		for(unsigned i = 0; i < size; ++i) {
			unsigned idx = (start + i) % size;
			if(idx == id) continue;
			lock_guard<mutex> lk(local_mtx[idx]);
			if(!local_tasks[idx].empty()) {
				function_t task = move(local_tasks[idx].front());
				local_tasks[idx].pop_front();
				return task;
			}
		}
		return nullptr;
	}

	bool all_empty() {
		for(unsigned i = 0; i < size; ++i) {
			if(!local_tasks[i].empty()) return false;
		}
		return true;
	}

	void work(unsigned id) {
		tl_my_id = static_cast<int>(id);

		while(true) {
			function_t task;

			// 1. 本地 LIFO pop_back
			{
				lock_guard<mutex> lk(local_mtx[id]);
				if(!local_tasks[id].empty()) {
					task = move(local_tasks[id].back());
					local_tasks[id].pop_back();
				}
			}

			// 2. 偷取：FIFO pop_front
			if(!task) {
				task = try_steal(id);
			}

			// 3. 有任务就执行，没任务就等待
			if(task) {
				task();
			} else {
				unique_lock<mutex> lk(global_mtx);
				// 再检查一次队列，避免在偷取失败和加锁之间丢任务
				if(!all_empty()) continue;
				if(done) break;
				// 超时等待：既不忙等，也不会在 shutdown 时卡死
				cv.wait_for(lk, chrono::milliseconds(1), [&] { return done.load(); });
				if(done && all_empty()) break;
			}
		}
	}
};

int main() {
	random_device rd;
	mt19937 mt(rd());
	{
		string task_example(const string &); // 前向声明

		size_t n = 1000000;
		uniform_int_distribution<int> uf(numeric_limits<int>::min(), numeric_limits<int>::max());

		vector<string> strs(n);
		for(size_t i = 0; i < n; ++i) {
			for(unsigned j = 0; j < 100; ++j) {
				strs[i] += to_string(uf(mt));
			}
		}

		// 再用 parallel for_each
		auto t0 = chrono::steady_clock::now();

		for_each(execution::par, strs.begin(), strs.end(), [&](const string &str) { return task_example(str); });

		auto t1 = chrono::steady_clock::now();

		for_each(strs.begin(), strs.end(), [&](const string &str) { return task_example(str); });

		auto t2 = chrono::steady_clock::now();

		ThreadPool tp;
		unsigned task_num = thread::hardware_concurrency() * 10;
		for(unsigned tid = 0; tid < task_num; ++tid) {
			unsigned start = (n / task_num) * tid;
			unsigned end = std::min(start + n / task_num, n); // ← 关键：min 兜底
			tp.submit([&, start, end] {
				for(unsigned i = start; i < end; ++i) {
					task_example(strs[i]);
				}
			});
		}
		tp.shutdown();

		auto t3 = chrono::steady_clock::now();

		cout << "并行运行时间:\t" << (t1 - t0).count() << endl;   // parallel for_each
		cout << "串行运行时间:\t" << (t2 - t1).count() << endl;   // serial for_each
		cout << "线程池运行时间:\t" << (t3 - t2).count() << endl; // thread
	}

	return 0;
}

string task_example(const string &s) {
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
