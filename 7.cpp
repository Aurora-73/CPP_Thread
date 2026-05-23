#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

// Promise & Future
/*
promise 存储用于异步检索的值 (类模板)
    template< class R > class promise; 基本模板。
    template< class R > class promise<R&>; 非 void 特化，用于线程间通信对象。
    template<> class promise<void>; void 特化，用于通信无状态事件。

packaged_task 封装一个函数，以异步检索其返回值 (类模板)

future 等待一个异步设置的值 (类模板)

shared_future 等待一个异步设置的值（可能被其他 future 引用）(类模板)

async 异步（可能在新线程中）运行一个函数并返回一个将保存结果的 std::future (函数模板)

launch 指定 std::async 的启动策略 (枚举)

future_status 指定在 std::future 和 std::shared_future 上执行的带超时等待的结果 (枚举)
*/

int main() {

	return 0;
}