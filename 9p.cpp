#include <atomic>
#include <condition_variable>
#include <exception>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

/* 实验版继承式泛型线程池：
1. submit 接收任意可调用对象和参数
2. 先构造 my::packaged_task<R()> 保留 future
3. 再额外包装成 my::packaged_task<void()>，统一放入队列
4. worker 线程只需要执行 void() 任务 */

namespace my {

template <typename Signature>
class packaged_task_base;

template <typename Res, typename... Args>
class packaged_task_base<Res(Args...)> {
public:
	virtual ~packaged_task_base() = default;

	packaged_task_base(const packaged_task_base &) = delete;
	packaged_task_base &operator=(const packaged_task_base &) = delete;

	packaged_task_base(packaged_task_base &&) noexcept = default;
	packaged_task_base &operator=(packaged_task_base &&) noexcept = default;

	bool valid() const noexcept {
		return static_cast<bool>(state_);
	}

	// future 从共享状态里的 promise 获取，因此同一个任务对象只能成功取一次 future。
	std::future<Res> get_future() {
		return state_->promise.get_future();
	}

	void reset() {
		ensure_state();
		// reset 的语义是”保留可调用对象，重建共享状态”，因此依赖 clone。
		state_ = state_->clone();
	}

protected:
	packaged_task_base() = default;

	struct state_base {
		// 每个任务都自带一个 promise，共享状态通过它暴露给 future。
		std::promise<Res> promise;

		virtual ~state_base() = default;
		virtual void run(Args... args) = 0;
		// clone 用于 reset：重新创建一个同类型任务状态，但共享状态是新的。
		virtual std::shared_ptr<state_base> clone() = 0;
	};

	explicit packaged_task_base(std::shared_ptr<state_base> state) noexcept : state_(std::move(state)) { }

	void ensure_state() const {
		if(!state_) {
			throw std::future_error(std::future_errc::no_state);
		}
	}

	std::shared_ptr<state_base> state_;
};

template <typename Signature>
class packaged_task;

template <typename Res, typename... Args>
class packaged_task<Res(Args...)> : public packaged_task_base<Res(Args...)> {
	using Base = packaged_task_base<Res(Args...)>;

	template <typename Fn>
	struct state_impl final : Base::state_base {
		template <typename FnArg>
		explicit state_impl(FnArg &&fn) : fn(std::forward<FnArg>(fn)) { }

		void run(Args... args) override {
			try {
				if constexpr(std::is_void_v<Res>) {
					// void 返回值的任务只需要执行函数，再把 promise 标记为就绪。
					std::invoke(fn, std::forward<Args>(args)...);
					this->promise.set_value();
				} else {
					// 非 void 返回值直接写入 promise。
					this->promise.set_value(std::invoke(fn, std::forward<Args>(args)...));
				}
			} catch(...) {
				// 与标准 packaged_task 一样，把异常转发到 future 侧。
				this->promise.set_exception(std::current_exception());
			}
		}

		std::shared_ptr<typename Base::state_base> clone() override {
			return std::make_shared<state_impl<Fn>>(std::move(fn));
		}

		Fn fn;
	};

	template <typename Fn, typename Fn2 = std::remove_cvref_t<Fn>>
	using not_same = std::enable_if_t<!std::is_same_v<packaged_task, Fn2>, int>;

public:
	packaged_task() noexcept = default;

	template <typename Fn, not_same<Fn> = 0>
	explicit packaged_task(Fn &&fn) : Base(std::make_shared<state_impl<std::decay_t<Fn>>>(std::forward<Fn>(fn))) { }

	packaged_task(const packaged_task &) = delete;
	packaged_task &operator=(const packaged_task &) = delete;
	packaged_task(packaged_task &&) noexcept = default;
	packaged_task &operator=(packaged_task &&) noexcept = default;

	using Base::get_future;
	using Base::reset;
	using Base::valid;

	void operator()(Args... args) {
		this->ensure_state();
		this->state_->run(std::forward<Args>(args)...);
	}
};

} // namespace my

using namespace std;
using namespace std::chrono_literals;

class ThreadPool {
public:
	ThreadPool(unsigned int n) {
		if(n == 0) {
			throw invalid_argument("thread count must be positive");
		}
		workers.reserve(n);
		for(unsigned int i = 0; i < n; ++i) {
			workers.emplace_back(&ThreadPool::worker_loop, this);
		}
	}

	ThreadPool() : ThreadPool(max(1u, std::thread::hardware_concurrency())) { }

	~ThreadPool() {
		shutdown();
	}

	ThreadPool(const ThreadPool &) = delete;
	ThreadPool &operator=(const ThreadPool &) = delete;

	template <typename F, typename... Args>
	future<std::invoke_result_t<F, Args...>> submit(F &&fun, Args &&...args) {
		using result_type = std::invoke_result_t<F, Args...>;

		if(stop_flag) {
			throw runtime_error("ThreadPool has been stopped");
		}

		auto callable = [fun = std::forward<F>(fun),
		                 tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> result_type {
			return std::apply(
			    [&fun](auto &&...xs) -> result_type {
				    return std::invoke(std::move(fun), std::forward<decltype(xs)>(xs)...);
			    },
			    std::move(tup));
		};

		auto task = std::make_unique<my::packaged_task<result_type()>>(std::move(callable));
		future<result_type> fu = task->get_future();

		{
			lock_guard<mutex> lk(mtx);
			if(stop_flag) {
				throw runtime_error("ThreadPool has been stopped");
			}
			tasks.emplace(make_void_task(std::move(task)));
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
		return remaining_tasks;
	}

private:
	template <typename R>
	static auto make_void_task(unique_ptr<my::packaged_task<R()>> task) {
		return make_unique<my::packaged_task<void()>>([task = std::move(task)]() mutable { (*task)(); });
	}

	void worker_loop() {
		while(true) {
			unique_lock<mutex> lk(mtx);
			cv.wait(lk, [&]() { return stop_flag || !tasks.empty(); });
			if(stop_flag && tasks.empty()) {
				break;
			}

			auto task = std::move(tasks.front());
			tasks.pop();
			lk.unlock();
			(*task)();
		}
	}

private:
	mutex mtx;
	atomic<bool> stop_flag = false;
	condition_variable cv;
	vector<thread> workers;
	queue<unique_ptr<my::packaged_task<void()>>> tasks;
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
