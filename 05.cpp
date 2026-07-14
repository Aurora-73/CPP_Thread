#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// 生产者消费者问题（Producer-consumer problem，也称有限缓冲问题）是多线程同步问题的经典案例。
// 该问题描述了两个共享固定大小缓冲区的线程——生产者与消费者在实际运行时的协同问题。
// 生产者负责生成数据存入缓冲区，消费者则从缓冲区取出数据消耗。
// 解决方案需通过记录型信号量确保：当缓冲区满时生产者停止写入，缓冲区空时消费者停止读取，从而实现线程间安全通信。

/*
生产者消费者问题：
    1、生产者在线程私有区并行地准备任务，在临界区内串行地把任务放入共享队列；
        消费者在临界区内串行地从共享队列取任务，在临界区外并行地处理任务。

    2、这里使用两个 condition_variable，分别对应两类等待谓词：
        - 生产者等待“队列未满”或“系统已停止继续生产”；
        - 消费者等待“队列非空”或“所有生产者都已退出”。
        condition_variable 本身不保存条件，真正的条件来自受保护的共享状态。

    3、任务队列以及与之相关的同步状态需要互斥访问，因此使用同一把 mutex
        保护它们，并让 wait(lock, pred) 在“检查条件”和“进入等待”之间保持原子语义。
        这样可以避免因为竞态导致线程错过状态变化。
        这里被共同保护的状态包括：
        - tasks_
        - 与队列等待/退出相关的状态判断

    4、退出分两步：
        - main 线程先设置 stop_produce，表示不再接收新的生产任务；
        - 各生产者陆续退出，最后一个生产者退出时唤醒所有消费者；
        - 消费者会继续处理队列中剩余的任务，直到发现”所有生产者都已退出且队列为空”后再结束。
        因此，系统能正确退出，不是因为 notify 次数多于任务数，
        而是因为消费者的等待谓词同时覆盖了”还有任务可做”和”不会再有新任务”这两种情况。

    5、wait 必须放在循环/谓词检查里，而不能假设”被唤醒就一定条件成立”，因为条件变量可能出现虚假唤醒。
       即cv.wait(lk, prod) 或 while(!prod) cv.wait()
*/

constexpr int max_size = 5;

std::queue<std::string> tasks;
std::atomic<int> task_id = 0;
std::atomic<bool> stop_produce = false; // 只用停生产者，消费者自己判断什么时候停止
std::atomic<int> active_producers = 0;  // 用于辅助消费者停止
std::mutex mtx;                         // 保护任务队列
std::condition_variable cv_not_full;    // 变量 (tasks 未满) 的挂起队列
std::condition_variable cv_not_empty;   // 变量 (tasks 非空) 的挂起队列

void producer(int i) {
	while(!stop_produce.load()) {
		int tid;
		{
			std::cout << std::format("[producer] {} 准备数据", i) << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 模拟数据准备
			tid = ++task_id;                                           // 模拟数据准备
		} // 并行区，生产者准备任务内容

		{
			std::unique_lock<std::mutex> lk(mtx);
			cv_not_full.wait(
			    lk, []() { return tasks.size() < max_size || stop_produce.load(); }); // 注意停止判断也要放在 cv 里

			// 停止生产后，不再把新准备的任务放入队列，避免停机时继续扩张任务量。
			if(stop_produce.load()) {
				break;
			}

			std::cout << std::format("[producer] {} 挂载任务", i) << std::endl;
			tasks.emplace(std::format("[task] {}", tid));
			lk.unlock(); // 先解锁后 notify_one 减少虚假唤醒
			cv_not_empty.notify_one();
		} // 串行区，生产者挂载任务
	}

	// 最后一个生产者负责广播“不会再有新任务”，让消费者在清空队列后退出。
	if(active_producers.fetch_sub(1) == 1) {
		cv_not_empty.notify_all();
	}
	std::cout << std::format("[producer] {} exit", i) << std::endl;
}

void consumer(int i) {
	while(true) {
		std::string str;
		{
			std::unique_lock<std::mutex> lk(mtx);
			cv_not_empty.wait(
			    lk, []() { return !tasks.empty() || active_producers.load() == 0; }); // 注意停止判断也要放在 cv 里

			// 没有生产者且队列已空，说明系统中的任务已经全部处理完成。
			if(tasks.empty() && active_producers.load() == 0) {
				break;
			}

			std::cout << std::format("[consumer] {} 获取任务", i) << std::endl;
			str = std::move(tasks.front());
			tasks.pop();
			lk.unlock(); // 先解锁后 notify_one 减少虚假唤醒
			cv_not_full.notify_one();
		} // 串行区，消费者取出任务

		{
			std::cout << std::format("[consumer] {} 处理任务", i) << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟任务处理
			std::cout << std::format("{} 执行完成", str) << std::endl;
		} // 并行区，消费者处理任务
	}
	std::cout << std::format("[consumer] {} exit", i) << std::endl;
}

int main() {
	constexpr int producer_count = 3;
	constexpr int consumer_count = 3;
	std::vector<std::thread> p_vec, c_vec;
	p_vec.reserve(producer_count);
	c_vec.reserve(consumer_count);

	for(int i = 0; i < producer_count; ++i) {
		++active_producers;
		p_vec.emplace_back(producer, i);
	}

	for(int i = 0; i < consumer_count; ++i) {
		c_vec.emplace_back(consumer, i);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// 这里只停止“继续生产新任务”，消费者会把队列中的剩余任务处理完再退出。
	stop_produce = true;
	cv_not_full.notify_all();

	for(int i = 0; i < producer_count; ++i) {
		p_vec[i].join();
		std::cout << std::format("[producer] {} exited", i) << std::endl;
	}

	for(int i = 0; i < consumer_count; ++i) {
		c_vec[i].join();
		std::cout << std::format("[consumer] {} exited", i) << std::endl;
	}

	// 所有工作线程结束后再读取队列状态，避免与并发访问形成数据竞争。
	{
		std::lock_guard<std::mutex> lk(mtx);
		std::cout << "结束时 剩余任务数: " << tasks.size() << std::endl;
		while(!tasks.empty()) {
			std::cout << std::format("缺少执行 {}", tasks.front()) << std::endl;
			tasks.pop();
		}
	}

	return 0;
}