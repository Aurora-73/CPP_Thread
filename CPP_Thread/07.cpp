#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

// Promise & Future
/*
promise 和 future 通过一个核心对象 shared_state（共享状态）关联。共享状态中保存了异常指针、结果值以及状态信息，promise 和 future 共同引用这个控制块。只有当共享状态不再被任何关联对象引用时，它才会被销毁。

promise 存储用于异步检索的值 (类模板) 
	每个 promise 都与一个共享状态关联，该共享状态包含状态信息和一个结果，该结果可能尚未就绪、已保存某个值（可能是 void），或已保存为异常。

    template<class R> class promise; 值模版
    template<class R> class promise<R&>; 引用模版
    template<> class promise<void>; void 特化，用于通信无状态事件。

	构造函数
		promise(); 
			默认构造函数。用空共享状态构造 promise。
		template< class Alloc > 
		promise( std::allocator_arg_t, const Alloc& alloc );
			构造一个带有空共享状态的 promise。共享状态使用 alloc 分配。Alloc 必须满足 Allocator 的要求。

		不支持拷贝构造和拷贝赋值，支持移动语义

	析构函数 
		如果共享状态已就绪，则释放它。
		如果共享状态未就绪，则存储一个类型为std::future_error、错误条件为std::future_errc::broken_promise的异常对象，使共享状态就绪并释放它。

    future<> get_future();
		返回与 *this 关联同一状态的未来体对象。 若 *this 无共享状态(已被移动走) 或已调用 get_future，则抛出异常 no_state / future_already_retrieved。
	
    set_value、set_exception、set_value_at_thread_exit 和 set_exception_at_thread_exit 的操作表现类似 。对这些成员函数的调用是互斥的。对这些函数的调用和对 get_future 的调用之间不会造成数据竞争（因此它们彼此不需要额外同步）。  
	
	void set_value(value);
        将 value 存储到共享状态中，并使状态就绪。若 *this 无共享状态，或共享状态已存储值或异常，则抛出std::future_error，其错误条件对应 no_state / promise_already_satisfied。                  

	void set_value_at_thread_exit(value);
        将 value 存储到共享状态中，但不立即使状态就绪。在当前线程退出时，销毁所有拥有线程局部存储期的对象后，再使状态就绪。

	void set_exception(p);
        将异常指针 p 存储到共享状态中，并使状态就绪。若 *this 无共享状态，或共享状态已存储值或异常，则抛出std::future_error，其错误条件对应 no_state / promise_already_satisfied。                  

	void set_exception_at_thread_exit(p);
        将异常指针 p 存储到共享状态中，但不立即使状态就绪。在当前线程退出时，销毁所有拥有线程局部 存储期的对象后，再使状态就绪。  

future 等待一个异步设置的值 (类模板)
	std::future 通常由 std::promise::get_future()、std::packaged_task::get_future() 或 std::async() 返回。 
	用于shared_state的接收端。std::future 是结果的唯一接收端访问对象，即该结果不与任何其他异步返回对象共享。使用 std::shared_future 来对结果进行非唯一访问。

	template<class T> class future;
	template<class T> class future<T&>;
	template<> class future<void>;

	默认构造函数 future(); 构造无共享状态的 std::future。构造后，valid() == false。
	支持移动语义，不支持拷贝构造和拷贝赋值

	析构函数
		释放任何共享状态。 如果当前对象持有其共享状态的最后一个引用，那么就会销毁共享状态。否则仅当前对象放弃它的共享状态的引用。

	bool valid() const noexcept;
		检查 future 是否仍然拥有 shared_state。
		- promise<int> p; auto f = p.get_future(); f.valid() 为 true;
		- 默认构造后无共享状态，f.valid() 为 false;                                                                                                                        
		- 被移动后无共享状态，f.valid() 为 false;                                                                                                                          
		- 调用 get() 或 share() 后失去共享状态，f.valid() 为 false; 
		除析构、移动赋值、valid() 外，不应在 valid() == false 的 future 上调用其他成员函数；实现通常会抛出 std::future_error。

	std::shared_future<T> share() noexcept;
		把“单次消费”的 future 转换成可复制、可多次 get() 的 shared_future。在 std::future 上调用 share 后 valid() == false。
		转移 *this 的共享状态到 std::shared_future 对象。多个 std::shared_future 对象可引用同一共享对象，而 std::future 不可以。
		返回含有先前 *this 所保有的共享状态（若存在）的 std::shared_future 对象，如同以 std::shared_future<T>(std:move(*this)) 构造。

	void wait() const;
		阻塞直至结果变得可用，不提取结果，也不会使 future 失效。若调用此函数前 valid() 为 false 通常抛出 std::future_error(no_state)。
	
	std::future_status wait_for( timeout_duration ) const;
	std::future_status wait_until( timeout_time ) const;
		等待结果变得可用。阻塞直至经过指定的 timeout_duration / 达到指定的 timeout_time，或结果变为可用，两者先达成者为止。返回值鉴别结果的状态。
		当共享状态来自 std::async(std::launch::deferred, ...) 时，wait_for / wait_until 不会真正等待异步执行完成，而是立即返回future_status::deferred。

		返回 future_status 枚举常量：
			future_status::deferred	共享状态包含一个使用惰性求值的延迟函数，仅在明确请求时才计算其结果
			future_status::ready	共享状态已就绪
			future_status::timeout	已超过时限

	T get();   (1)
	T& get();   (2)
	void get();  (3)
		阻塞直到共享状态就绪，然后提取结果。                                                                                                    
		(1) 返回以 std::move(v) 在共享状态中存储的值 v。                                                                             
		(2) 返回在共享状态中作为值存储的引用。                                                                                                          
		(3) 无返回值，仅等待其完成。                                                                                                            
		如果共享状态中保存的是异常，则重新抛出该异常。 如果调用 get 前 valid 为 false 通常抛异常                                                                                    
		调用 get() 后，future 不再有效，valid() 为 false。 

shared_future 等待一个异步设置的值（支持多个对象共享同一个 shared_state）(类模板)
	future 不支持拷贝构造和拷贝赋值，且调用 get 后 valid 置为false，shared_future 支持多个对象共享同一个 shared_state，且调用 get 后不会置 valid 为 false

	构造函数：
		shared_future() 无参构造，构造不指代共享状态，即 valid() 为 false 的 shared_future。
		shared_future( std::future<T>&& other ) noexcept; 转移 other 所保有的共享状态给 *this。构造后，other.valid() == false 且 this->valid() == true（若 other 之前有效）。
		支持拷贝构造/赋值和移动构造/赋值
	析构函数：
		若 *this 是指代共享状态的最后一个对象，则销毁共享状态。

	const T& get() const;   (1)
	T& get();   (2)
	void get();  (3)
		阻塞直到共享状态就绪，然后提取结果。                                                                                                    
		(1) 返回在共享状态中存储的值的 const 引用。销毁共享状态后，通过此引用访问值的行为未定义。
		(2) 返回在共享状态中作为值存储的引用。                                                                                                       
		(3) 无返回值，仅等待其完成。                                                                                                            
		如果共享状态中保存的是异常，则重新抛出该异常。 如果调用 get 前 valid 为 false 通常抛异常                                                                                       
		调用 get() 后，shared_future 仍然有效
	
	valid、wait、wait_for、wait_until 函数同 future
*/

using namespace std;

void pause() {
	cout << "Press Enter to continue..." << endl;
	cin.get();
}

int main() {

	{
		promise<int> *p = new promise<int>();
		future f = p->get_future();
		jthread t([&]() {
			try {
				cout << f.get() << endl;
			} catch(const exception &e) {
				cout << "共享状态为异常" << endl;
				cout << e.what() << endl;
			}
		});

		cout << "promise 析构时共享状态未就绪，将共享状态设为异常" << endl;
		delete p;
	} // promise 析构时如果共享状态未就绪，则设置为异常

	pause();

	{
		promise<int &> p;
		future f = p.get_future();
		int val = 0;
		jthread t([&] { p.set_value(val); });
		f.get() = 2;
		cout << val << endl;
	} // template<class R> class promise<R&>; 是引用模版

	pause();

	{
		promise<int> p;
		shared_future f(p.get_future());              // 使用 future 构造 shared_future，直接会移动共享状态所有权
		jthread s1([f] { cout << f.get() << endl; }); // 以值方式捕获，lamda表达式内会拷贝一份
		jthread s2([f] { cout << f.get() << endl; });
		jthread s3([f] { cout << f.get() << endl; });
		jthread t([&] { p.set_value(3); });
	} // shared_future

	return 0;
}