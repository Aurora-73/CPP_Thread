#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <queue>
#include <condition_variable>

// 生产者消费者模型

using namespace std;

// 定义缓冲区最大容量
const int BUFFER_SIZE = 10;

// 共享数据和同步原语
queue<int> buffer;
mutex mtx;
condition_variable cv_producer; // 生产者条件变量
condition_variable cv_consumer; // 消费者条件变量
bool done = false; // 生产结束标志
// 记录生产者数量，用于判断何时所有生产者都完成
int producer_count = 2;

// 生产者函数
void producer(int id, int items) {
	for(int i = 1; i <= items; ++i) {
		// 模拟生产过程
		this_thread::sleep_for(chrono::milliseconds(200));

		{
			// 获取锁
			unique_lock<mutex> lock(mtx);

			// 如果缓冲区已满，等待消费者消费
			cv_producer.wait(lock, [] { return buffer.size() < BUFFER_SIZE; });

			// 生产数据
			int item = i + (id * 100);
			buffer.push(item);
			cout << "生产者 " << id << " 生产了: " << item << ", 缓冲区大小: " << buffer.size() << endl;

			// 通知消费者可以消费了
			cv_consumer.notify_one();
		}
	}

	{
		lock_guard<mutex> lock(mtx);
		cout << "生产者 " << id << " 完成生产" << endl;
		if(--producer_count == 0) {
			done = true;
			cv_consumer.notify_all(); // 通知所有消费者生产已结束
		}
	}
}

// 消费者函数
void consumer(int id) {
	while(true) {
		// 模拟消费过程的时间
		this_thread::sleep_for(chrono::milliseconds(300));

		{
			// 获取锁
			unique_lock<mutex> lock(mtx);

			// 如果缓冲区为空且生产未结束，等待生产者生产
			cv_consumer.wait(lock, [] { return !buffer.empty() || done; });

			// 如果缓冲区为空且生产已结束，退出循环
			if(buffer.empty() && done) {
				cout << "消费者 " << id << " 退出" << endl;
				break;
			}

			// 消费数据
			int item = buffer.front();
			buffer.pop();
			cout << "消费者 " << id << " 消费了: " << item << ", 缓冲区大小: " << buffer.size() << endl;

			// 通知生产者可以生产了
			cv_producer.notify_one();
		}
	}
}

int main() {
	// 创建2个生产者线程和3个消费者线程
	vector<thread> threads;

	// 启动消费者线程
	for(int i = 1; i <= 3; ++i) {
		threads.push_back(thread(consumer, i));
	}

	// 启动生产者线程
	threads.push_back(thread(producer, 1, 15));
	threads.push_back(thread(producer, 2, 10));

	// 等待所有线程完成
	for(auto &t : threads) {
		t.join();
	}

	cout << "所有线程已完成工作" << endl;
	return 0;
}
