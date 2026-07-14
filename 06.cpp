#include <chrono>
#include <condition_variable>
#include <format>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// 生产者消费者问题 类实现
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
class TaskQueue {
public:
	explicit TaskQueue(int max_size, int producer_count) : max_size_(max_size), active_producers_(producer_count) { }

	void request_stop_producing() {
		std::lock_guard<std::mutex> lk(mtx_);
		stop_produce_ = true;
		cv_not_full_.notify_all();
	}

	bool push_task(std::string task) {
		std::unique_lock<std::mutex> lk(mtx_);
		cv_not_full_.wait(lk, [this]() { return tasks_.size() < max_size_ || stop_produce_; });

		// 一旦进入“停止接收新任务”状态，即使任务已经在临界区外准备好了，也不再入队。
		if(stop_produce_) {
			return false;
		}

		tasks_.emplace(std::move(task));
		lk.unlock();
		cv_not_empty_.notify_one();
		return true;
	}

	bool pop_task(std::string &task) {
		std::unique_lock<std::mutex> lk(mtx_);
		cv_not_empty_.wait(lk, [this]() { return !tasks_.empty() || active_producers_ == 0; });

		// 没有生产者且队列已空，说明系统中的任务已经全部处理完成。
		if(tasks_.empty() && active_producers_ == 0) {
			return false;
		}

		task = std::move(tasks_.front());
		tasks_.pop();
		lk.unlock();
		cv_not_full_.notify_one();
		return true;
	}

	void producer_exit() {
		std::lock_guard<std::mutex> lk(mtx_);
		// 最后一个生产者负责广播“不会再有新任务”，让消费者在清空队列后退出。
		--active_producers_;
		if(active_producers_ == 0) {
			cv_not_empty_.notify_all();
		}
	}

	std::size_t remaining_tasks() const {
		std::lock_guard<std::mutex> lk(mtx_);
		return tasks_.size();
	}

private:
	mutable std::mutex mtx_;               // 保护任务队列及其配套条件变量依赖的状态
	std::condition_variable cv_not_full_;  // 队列未满时唤醒生产者
	std::condition_variable cv_not_empty_; // 队列非空时唤醒消费者
	std::queue<std::string> tasks_;
	const int max_size_;
	bool stop_produce_ = false;
	int active_producers_;
};

constexpr int producer_count = 3;
constexpr int consumer_count = 3;

TaskQueue task_queue(5, producer_count);
std::mutex task_id_mtx;
int task_id = 0;

int next_task_id() {
	std::lock_guard<std::mutex> lk(task_id_mtx);
	return ++task_id;
}

void producer(int i) {
	while(true) {
		int tid;
		{
			std::cout << std::format("[producer] {} 准备数据", i) << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 模拟数据准备
			tid = next_task_id();
		} // 并行区，生产者准备任务内容

		std::cout << std::format("[producer] {} 挂载任务", i) << std::endl;
		if(!task_queue.push_task(std::format("[task] {}", tid))) {
			break;
		}
	}

	task_queue.producer_exit();
	std::cout << std::format("[producer] {} exit", i) << std::endl;
}

void consumer(int i) {
	while(true) {
		std::string str;
		if(!task_queue.pop_task(str)) {
			break;
		}

		std::cout << std::format("[consumer] {} 获取任务", i) << std::endl;
		{
			std::cout << std::format("[consumer] {} 处理任务", i) << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟任务处理
			std::cout << std::format("{} 执行完成", str) << std::endl;
		} // 并行区，消费者处理任务
	}
	std::cout << std::format("[consumer] {} exit", i) << std::endl;
}

int main() {
	std::vector<std::thread> p_vec, c_vec;
	p_vec.reserve(producer_count);
	c_vec.reserve(consumer_count);

	for(int i = 0; i < producer_count; ++i) {
		p_vec.emplace_back(producer, i);
	}

	for(int i = 0; i < consumer_count; ++i) {
		c_vec.emplace_back(consumer, i);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// 这里只停止“继续生产新任务”，消费者会把队列中的剩余任务处理完再退出。
	task_queue.request_stop_producing();

	for(int i = 0; i < producer_count; ++i) {
		p_vec[i].join();
		std::cout << std::format("[producer] {} exited", i) << std::endl;
	}

	for(int i = 0; i < consumer_count; ++i) {
		c_vec[i].join();
		std::cout << std::format("[consumer] {} exited", i) << std::endl;
	}

	// 所有工作线程结束后再读取队列状态，避免与并发访问形成数据竞争。
	std::cout << "剩余任务数: " << task_queue.remaining_tasks() << std::endl;

	return 0;
}
