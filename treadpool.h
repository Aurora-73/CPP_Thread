#pragma once
#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

// 任务窃取线程池
/*
  1. 整体架构：Work-Stealing

  传统线程池用一个全局队列 + 一把锁，所有线程竞争同一把锁，高并发下成为瓶颈。

  这个线程池用 per-thread 本地队列 + work-stealing：
    每个 worker 有自己的 deque + 自己的 mutex，本地操作无全局锁竞争
    空闲的 worker 随机偷取其他 worker 的任务
    外部线程（非 worker）提交任务时 round-robin 分配到某个队列

  ---
  2. 成员变量

  const unsigned size;                         // 线程数
  std::atomic<bool> done = false;              // 关闭标志
  std::atomic<bool> has_work = false;          // 任务通知信号（替代 condition_variable）
  std::atomic<unsigned> curr_id = 0;           // 外部线程 round-robin 计数器
  inline static thread_local int tl_my_id = -1;// 当前线程是否是 worker，-1 表示不是

  std::vector<std::deque<function_t>> local_tasks;  // 每个 worker 的任务队列
  std::vector<std::mutex> local_mtx;                // 每个队列一把锁
  std::vector<std::jthread> threads;                // worker 线程（析构自动 join）

  关键设计决策：
    用 deque 而不是 queue，因为需要双端操作：本地从 back 取（LIFO），偷取从 front 取（FIFO）
    用 jthread 而不是 thread，析构时自动 join，不会因忘记 join 而 terminate
    tl_my_id 用 thread_local 区分 worker 线程和外部线程

  ---
  3. submit 流程

  template <typename F, typename... Args>
  auto submit(F &&f, Args &&...args) {

  第一步：决定提交到哪个队列

  if(tl_my_id != -1)    // worker 线程提交到自己的队列
      id = tl_my_id;
  else                  // 外部线程 round-robin
      id = curr_id.fetch_add(1) % size;

  worker 提交到自己的队列有缓存亲和性优势——取出来的任务大概率还在 L1/L2 缓存里。

  第二步：类型擦除，包装成 function_t

  C++23 路径（move_only_function）：

  std::packaged_task<return_t(Args...)> pt(std::forward<F>(f));
  std::future<return_t> res = pt.get_future();
  // 包装成 void() lambda，捕获 packaged_task + 参数元组
  local_tasks[id].emplace_back(
      [pt = std::move(pt), tup = make_tuple(forward<Args>(args)...)]() mutable {
          apply(std::ref(pt), std::move(tup));  // 解包元组调用 pt
      });

  C++20 路径（std::function 需要可拷贝）：

  auto pt = std::make_shared<packaged_task<return_t(Args...)>>(forward<F>(f));
  // shared_ptr 包装，让 lambda 可拷贝

  packaged_task 把返回值通过 future 传出。submit 最后返回 future，调用者可以 get() 阻塞等待结果。

  第三步：唤醒 worker

  has_work.store(true, std::memory_order_release);
  has_work.notify_one();

  只唤醒一个。如果有多个空闲 worker，后续的 submit 会逐个唤醒。

  ---
  4. worker 主循环

  void work(unsigned id) {
      tl_my_id = static_cast<int>(id);  // 标记自己是 worker
      while(true) {
          function_t task;

          // ① 本地取：LIFO pop_back
          { lock_guard lk(local_mtx[id]);
            if(!local_tasks[id].empty()) {
                task = move(local_tasks[id].back());
                local_tasks[id].pop_back();
            }
          }

          // ② 偷取：FIFO pop_front
          if(!task) task = try_steal(id);

          // ③ 三种情况
          if(task)    task();              // 有任务 → 执行
          else if(done) break;            // 关闭 → 退出
          else {                          // 没任务 → 等待
              has_work.store(false, relaxed);
              has_work.wait(false);       // 阻塞直到 notify
          }
      }
  }

  为什么本地 LIFO、偷取 FIFO？

  这是 Cilk 调度器的经典策略：

    本地 LIFO：最近提交的任务最可能还在缓存中（空间局部性），而且递归分解的子任务"先提交的先完成"符合深度优先语义
    偷取 FIFO：偷取的是"最老"的任务，离当前活跃任务最远，减少争抢概率

  has_work 等待协议

  store(false)  ←── 防止虚假唤醒：flag 变 false，如果没人 set true 就不醒
  wait(false)   ←── flag==false 时真正阻塞

  为什么不会丢失唤醒：

  时间线 A（安全）：
    worker:  store(false) → wait(false) 阻塞
    submit:  store(true)  → notify_one()  → wait 返回

  时间线 B（也是安全的）：
    worker:  store(false)
    submit:  store(true)  → notify_one()  ← 在 store 和 wait 之间发生
    worker:  wait(false)  ← flag 已是 true，立即返回不阻塞

  ---
  5. try_steal — 偷取

  function_t try_steal(unsigned id) {
      static thread_local mt19937 rng(random_device{}());
      unsigned start = rng() % size;       // 随机起点
      for(unsigned i = 0; i < size; ++i) {
          unsigned idx = (start + i) % size;
          if(idx == id) continue;          // 不偷自己
          lock_guard lk(local_mtx[idx]);
          if(!local_tasks[idx].empty()) {
              task = move(local_tasks[idx].front());  // FIFO
              local_tasks[idx].pop_front();
              return task;
          }
      }
      return nullptr;
  }

    随机起点避免所有小偷都从 queue[0] 开始，导致第一个队列被反复锁
    每次偷一个任务，不批量偷取——简单且避免长时间持锁

  ---
  6. shutdown 与 shutdown_now

  shutdown — 优雅关闭

  void shutdown() {
      if(done.exchange(true)) return;  // 防止重复调用
      has_work.notify_all();           // 唤醒所有阻塞的 worker
      for(auto &th : threads) th.join(); // 等待所有任务执行完
  }

  流程：设 done=true → 唤醒 worker → worker 发现 done 且无新任务就退出 → join 等所有人结束。已入队的任务全部执行完。

  shutdown_now — 立即关闭

  vector<function_t> shutdown_now() {
      if(done.exchange(true)) return {};
      auto remaining = drain();        // 抢先把所有队列清空
      has_work.notify_all();
      for(auto &th : threads) th.join();
      return remaining;                // 返回未执行的任务
  }

  流程：设 done=true → drain() 清空所有队列 → 唤醒 worker → worker 发现 done 且队列空就退出。

  注意：drain() 之后、join() 之前，某个 worker 可能刚从队列取出一个任务正在执行中。这个任务会被执行完（join 等它结束），但不会出现在 remaining
  中——它属于"正在执行"而非"未执行"。这语义是正确的。

  ---
  7. 与传统单队列线程池的对比

  ┌──────────┬──────────────────┬──────────────────────────────────────┐
  │          │    单全局队列     │       本实现（work-stealing）         │
  ├──────────┼──────────────────┼──────────────────────────────────────┤
  │ 锁竞争   │ 所有线程抢一把锁  │ 本地操作无全局锁，偷取时各锁各的         │
  ├──────────┼──────────────────┼──────────────────────────────────────┤
  │ 缓存亲和  │ 任务随机分配     │ 本地 LIFO 保持缓存热度                 │
  ├──────────┼──────────────────┼──────────────────────────────────────┤
  │ 负载均衡  │ 天然均衡         │ 空闲线程主动偷取                       │
  ├──────────┼──────────────────┼──────────────────────────────────────┤
  │ 复杂度    │ 简单             │ 较复杂，但这是工业级线程池的标准做法    │
  └──────────┴──────────────────┴──────────────────────────────────────┘
*/

class ThreadPool {
public:
#if __cplusplus > 202002L && _GLIBCXX_HOSTED
	using function_t = std::move_only_function<void()>;
#else
	using function_t = std::function<void()>;
#endif

	explicit ThreadPool(unsigned n) : size(n) {
		local_tasks.resize(n);
		local_mtx.resize(n);
		threads.reserve(n);
		for(unsigned i = 0; i < n; ++i) {
			threads.emplace_back(&ThreadPool::work, this, i);
		}
	}

	ThreadPool() : ThreadPool(std::max(std::thread::hardware_concurrency(), 2u)) { }

	ThreadPool(ThreadPool &&) = delete;
	ThreadPool(const ThreadPool &) = delete;

	template <typename F, typename... Args>
	auto submit(F &&f, Args &&...args) {
		using return_t = std::invoke_result_t<F, Args...>;

		if(done) throw std::runtime_error("ThreadPool has been stopped");

		// worker 线程提交到自己的本地队列（LIFO），外部线程 round-robin
		unsigned id;
		if(tl_my_id != -1) {
			id = static_cast<unsigned>(tl_my_id);
		} else {
			id = curr_id.fetch_add(1) % size;
		}

#if __cplusplus > 202002L && _GLIBCXX_HOSTED
		std::packaged_task<return_t(Args...)> pt(std::forward<F>(f));
		std::future<return_t> res = pt.get_future();
		{
			std::lock_guard<std::mutex> lk(local_mtx[id]);
			local_tasks[id].emplace_back(
			    [pt = std::move(pt), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
				    std::apply(std::ref(pt), std::move(tup));
			    });
		}
#else
		auto pt = std::make_shared<std::packaged_task<return_t(Args...)>>(std::forward<F>(f));
		std::future<return_t> res = pt->get_future();
		{
			std::lock_guard<std::mutex> lk(local_mtx[id]);
			local_tasks[id].emplace_back([pt, tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
				std::apply(*pt, std::move(tup));
			});
		}
#endif

		has_work.store(true, std::memory_order_release);
		has_work.notify_one();
		return res;
	}

	bool is_exit() {
		return done.load();
	}

	void shutdown() {
		if(done.exchange(true)) return;
		has_work.notify_all();
		for(auto &th : threads) th.join();
	}

	std::vector<function_t> shutdown_now() {
		std::vector<function_t> remaining;
		if(done.exchange(true)) return remaining;
		remaining = drain();
		has_work.notify_all();
		for(auto &th : threads) th.join();
		return remaining;
	}

private:
	const unsigned size;
	std::atomic<bool> done = false;
	std::atomic<bool> has_work = false;
	std::atomic<unsigned> curr_id = 0;
	inline static thread_local int tl_my_id = -1;

	std::vector<std::deque<function_t>> local_tasks; // owner: back 进 back 出 (LIFO)
	std::vector<std::mutex> local_mtx;
	std::vector<std::jthread> threads;

	// 偷取：从别人队列的 front 取（FIFO）
	function_t try_steal(unsigned id) {
		// 随机起点避免所有 thief 从同一个队列开始争抢
		static thread_local std::mt19937 rng(std::random_device {}());
		unsigned start = rng() % size;
		for(unsigned i = 0; i < size; ++i) {
			unsigned idx = (start + i) % size;
			if(idx == id) continue;
			std::lock_guard<std::mutex> lk(local_mtx[idx]);
			if(!local_tasks[idx].empty()) {
				function_t task = std::move(local_tasks[idx].front());
				local_tasks[idx].pop_front();
				return task;
			}
		}
		return nullptr;
	}

	// 收集所有队列中未执行的任务（调用前需确保没有并发访问）
	std::vector<function_t> drain() {
		std::vector<function_t> remaining;
		for(unsigned i = 0; i < size; ++i) {
			std::lock_guard<std::mutex> lk(local_mtx[i]);
			while(!local_tasks[i].empty()) {
				remaining.emplace_back(std::move(local_tasks[i].front()));
				local_tasks[i].pop_front();
			}
		}
		return remaining;
	}

	void work(unsigned id) {
		tl_my_id = static_cast<int>(id);

		while(true) {
			function_t task;

			// 1. 本地 LIFO pop_back
			{
				std::lock_guard<std::mutex> lk(local_mtx[id]);
				if(!local_tasks[id].empty()) {
					task = std::move(local_tasks[id].back());
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
			} else if(done) {
				break;
			} else {
				// atomic wait 标准模式：
				// 1. 清除 has_work 防止虚假唤醒
				// 2. 没拿到任务时等待通知
				// 3. 循环重新检查队列，不丢失唤醒
				has_work.store(false, std::memory_order_relaxed);
				has_work.wait(false);
			}
		}
	}
};
