#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>

// packaged_task & async
/*
packaged_task 封装一个函数，以异步检索其返回值 (类模板)
	template< class R, class ...Args > class packaged_task<R(Args...)>;
	类模板 std::packaged_task 包装任何可调用 (Callable) 目标（函数、lambda 表达式、bind 表达式或其他函数对象），使得能异步调用它。其返回值或所抛异常被存储于能通过 std::future 对象访问的共享状态中。
	
	构造函数：
		packaged_task() noexcept;   (1)
		explicit packaged_task( F&& f );   (2)
		explicit packaged_task( std::allocator_arg_t, const Allocator& a, F&& f );  (3)
		(1) 构造无任务且无共享状态的 std::packaged_task 对象。
		(2) 构造拥有 f 任务和共享状态的 std::packaged_task 对象，以 std::forward<F>(f) 初始化任务并存储。 (传入的f为右值则会被移动)
		(3) 用分配器 a 分配存储任务所需的内存。
	支持移动语义，不支持拷贝构造

	bool valid() const noexcept; 检查 *this 是否拥有共享状态。

	std::future<R> get_future();
		返回与 *this 共享同一共享状态的 future。对每个 packaged_task 只能调用一次 get_future。
		异常：若已通过调用 get_future 取得共享状态，抛出异常 future_already_retrieved。 若 *this 无共享状态，抛出异常 no_state。

	void operator()( Args... args );
		如同以 INVOKE<R>(f, args...) 调用存储的任务 f。任务返回值或任何抛出的异常被存储于共享状态。令共享状态就绪，并解除阻塞任何等待此操作的线程。
		异常：存储的任务已经被调用过。此时设置错误类别为 promise_already_satisfied。*this 没有共享状态。此时设置错误类别为 no_state。

	void make_ready_at_thread_exit( Args... args );
		如同以 INVOKE<R>(f, args...) 调用存储的任务 f。任务返回值或任何抛出的异常被存储于 *this 的共享状态。
		只有在在当前线程退出，并销毁所有线程局域存储期对象后，共享状态才会就绪。
		异常：存储的任务已经被调用过。此时设置错误类别为 promise_already_satisfied。*this 没有共享状态。此时设置错误类别为 no_state。

	void reset();
		重置状态，抛弃先前执行的结果。构造新的共享状态。
		等价于 *this = packaged_task(std::move(f))，其中 f 是存储的任务。
		异常：若 *this 无共享状态则为 std::future_error。设置错误条件为 no_state。若无足够内存以分配新的共享状态则为 std::bad_alloc。

	packaged_task 类本身不负责并发，它负责把“函数执行结果”接入 future 同步模型。也就是将函数的返回值包装为future变量，调用函数后设置future的值。
	普通的线程执行会直接抛弃返回值，而包装为 packaged_task 后可以通过 future 获取返回值或异常。

async 异步（可能在新线程中）运行一个函数并返回一个将保存结果的 std::future (函数模板)
	std::future<V> async( F&& f, Args&&... args );  (1)
	std::future<V> async( std::launch policy, F&& f, Args&&... args );  (2)
		其中 V 为 std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>
		(1) 表现如同以 std::launch::async | std::launch::deferred 作为 policy 调用 (2)。
		(2) 按照特定的启动策略 policy（见下文），以参数 args 调用函数 f。

	launch 指定 std::async 的启动策略 (枚举)
		enum class launch { async = 1, deferred = 2 };
		策略	是否创建线程	任务何时运行		谁执行
		async		是			async调用时		   新线程
		deferred	否		第一次wait/get时	调用wait/get的线程

	如果有设置异步标志，即 (policy & std::launch::async) != 0，那么 std::async 会如同在一个以 std::thread 对象表示的新执行线程中调用
		INVOKE(decay-copy(std::forward<F>(f)), decay-copy(std::forward<Args>(args))...)
	如果有设置推迟标志（即 (policy & std::launch::deferred) != 0），那么 std::async 会将decay-copy(std::forward<F>(f)) 和 
		decay-copy(std::forward<Args>(args))...存储到共享状态中。进行惰性求值：
		在 std::async 返回给的 std::future 上首次调用非定时等待函数时会在调用等待函数的线程中求值 INVOKE
	如果同时设置两种标志，即 policy 是 std::launch::async | std::launch::deferred，那么会优先创建新线程，如果无法创建新线程就会回退到推迟调用或其他由实现定义的策略。

 	将结果或异常置于关联到该 std::future 的共享状态，然后才令它就绪。对同一 std::future 的所有后续访问都会立即返回结果。
	如果从 std::async 获得的 std::future 没有被移动或绑定到引用，那么在完整表达式结尾， std::future 的析构函数将阻塞到异步计算完成，实质上令如下代码同步：
	以调用 std::async 以外的方式获得的 std::future 的析构函数不会阻塞。
	对于 deferred 策略，如果始终没有调用 get/wait，则函数根本不会被执行，future 析构时直接丢弃。
*/

using namespace std;

void pause() {
	cout << "Press Enter to continue..." << endl;
	cin.get();
}

int main() {
	{
		string s = "Hello";
		auto task = [s = std::move(s)]() -> string {
			cout << "sleep for 0.1 second" << endl;
			this_thread::sleep_for(100ms);
			return s + " World";
		};

		packaged_task<string()> p_task(std::move(task));
		future<string> fu = p_task.get_future();
		jthread th(std::ref(p_task));
		// 注意不能直接写jthread th(p_task); 因为线程内部会拷贝一次p_task，而p_task没有拷贝构造和拷贝赋值
		// jthread th(packaged_task<string()>::operator(), &p_task); 也可以
		cout << fu.get() << endl;

	} // packaged_task示例

	pause();

	{
		string arg = "Hello";
		auto task = [](string &s) -> string {
			cout << "sleep for 1 second" << endl;
			this_thread::sleep_for(1s);
			return s + " World";
		};
		packaged_task<string(string &)> p_task(task); // 需要带引用类型，构造时只加入函数，不加入参数
		future<string> fu = p_task.get_future();
		jthread th(std::ref(p_task), std::ref(arg)); // 运行时才传入参数
		cout << fu.get() << endl;
	} // packaged_task示例，带参数

	pause();

	{
		cout << "主线程id " << this_thread::get_id() << endl;
		auto print_id = [](const string &s) {
			cout << s << " 调用线程id " << this_thread::get_id() << endl;
		};

		future fu_a = async(launch::async, std::ref(print_id), "launch::async");
		fu_a.get();

		future fu_d = async(launch::deferred, std::ref(print_id), "launch::deferred");
		fu_d.get();

		future fu_n = async(std::ref(print_id), "none_policy");
		fu_n.get();
	}

	return 0;
}