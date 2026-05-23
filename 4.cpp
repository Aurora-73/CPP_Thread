#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <iostream>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

// 条件变量，是一种同步原语，允许多个线程相互通信。它允许一定数量的线程等待（可能带超时）来自另一个线程的通知，表明它们可以继续。条件变量总是与互斥体关联。在头文件 <condition_variable> 中定义
/*
condition_variable(类) 提供与 std::unique_lock 关联的条件变量
    只有默认的无参构造，不支持拷贝和移动

    __condvar _M_cond; 
        核心成员变量，是底层真正的条件变量对象，通常映射到操作系统的线程同步原语：
        Linux / POSIX 系统用 pthread_cond_t，Windows 系统用 CONDITION_VARIABLE，它负责维护等待线程队列。

    void wait(unique_lock<mutex>& __lock);
        wait 的执行过程是原子性的 { 调用 __lock.unlock()，把当前线程挂到 _M_cond 的等待队列，阻塞线程 } ，不会在解锁和进入等待之间出现中间状态（这是 CV 存在的核心理由）。
        （让操作系统调度其他线程解除阻塞，其他线程调用notify_*() 会使至少一个/所有等待线程变为“可运行状态”（ready）)
        当解除阻塞时，调用 __lock.lock()（可能会在该锁上阻塞），然后wait函数返回，线程继续执行
        __lock 的释放和再次获取是waith函数自动管理的，线程会在函数返回前抢到锁，抢不到就在锁上阻塞。

    void wait(unique_lock<mutex>& __lock, _Predicate __p) {	while (!__p()) wait(__lock); };
        这是标准实现方式：循环检查 predicate，因为即使线程被唤醒，也可能是“虚假唤醒”（spurious wakeup），所以标准要求必须循环判断条件，确保安全。
        condition_variable 不保存状态，它只负责“通知变化发生过”，维护挂起队列

    void notify_one() noexcept { _M_cond.notify_one(); }
    void notify_all() noexcept { _M_cond.notify_all(); }
        调用底层操作系统的条件变量接口：
            notify_one() 唤醒等待队列里的一个线程，
            notify_all() 唤醒所有等待线程（但是这些线程间由于需要竞争锁还是只能串行执行）
        注意：唤醒并不意味着线程立即执行，它们还必须抢占 mutex 才能继续。
        通知线程不需要持有与等待线程所持有的相同互斥锁。实际上这样做可能会是一种性能下降，因为被通知的线程会由于未持有锁而立即再次阻塞，等待通知线程释放锁。

    std::cv_status wait_for( std::unique_lock<std::mutex>& lock, const std::chrono::duration& rel_time ); (1)
    bool wait_for( std::unique_lock<std::mutex>& lock, const std::chrono::duration& rel_time, Predicate pred ); (2)
    std::cv_status wait_until( std::unique_lock<std::mutex>& lock, const std::chrono::time_point<Clock, Duration>& abs_time ); (1)
    bool wait_until( std::unique_lock<std::mutex>& lock, const std::chrono::time_point<Clock, Duration>& abs_time, Predicate pred ); (2)
    在wait的基础上，如果已到达等待时间限制或已到达等待时间点则唤醒
    返回值
        (1) 如果达到 abs_time，则返回 std::cv_status::timeout，否则返回 std::cv_status::no_timeout。
        (2) 在返回给调用者之前，pred() 的最新结果。

    unique_lock<mutex> 与 condition_variable 的关系：
        mutex：保护条件状态（你的 val、队列等）
        condition_variable：管理阻塞等待的线程队列
        协作方式：
            先 lock(mutex) （手动，一般用 unique_lock<mutex> lk(mtx);），然后检查条件
            如果条件不满足，wait(lk) → 自动解锁并挂起，其他线程修改条件，通知线程唤醒 notify_*()
            被唤醒线程重新获取 mutex，继续检查条件（while (!__p()) 避免虚假唤醒 ）
            所以 mutex 并不是 condition_variable 的一部分，它只保证条件的互斥访问。
            条件变量的规范要求：所有等待同一个 condition_variable 的线程必须在等待前持有同一把互斥锁，并且 wait() 会自动解锁这把互斥锁，然后阻塞。

condition_variable_any(类) 提供与任意锁类型关联的条件变量
    只有默认的无参构造，不支持拷贝和移动
    condition_variable_any 的 wait 函数是一个模版函数，支持 BasicLockable 的锁，比如 shared_lock 等
    std::condition_variable_any 可以与 std::shared_lock 一起使用，以在共享所有权模式下等待 std::shared_mutex。
    std::condition_variable_any 与自定义 Lockable 类型的可能用途是提供方便的、可中断的等待：自定义锁操作将按预期锁定关联的互斥体，并执行必要的设置以在收到中断信号时通知此条件变量。

    bool wait( Lock& lock, std::stop_token stoken, Predicate pred );
        (C++20) 解决传统 CV 没有“外部取消机制”stop_token = “额外唤醒源 + 退出条件” 的问题，允许等待中的线程可以被“主动取消”

notify_all_at_thread_exit(函数) 调度在当前线程完全结束后调用 notify_all
    void notify_all_at_thread_exit( std::condition_variable& cond, std::unique_lock<std::mutex> lk );
    用于通知其他线程给定线程已完全完成，包括销毁所有 thread_local 对象。它保证了通知将在线程对象被销毁、本地变量被清理、std::thread 内部状态被更新之后，但在操作系统线程实际终止之前发生。
    与线程函数最后执行lk.unlock(); cv.notify_all(); 的区别在于notify_all_at_thread_exit是在 thread_local 对象销毁后进行的通知，而普通情况下 thread_local 的析构发生在线程函数最后一条语句执行结束后。

cv_status(枚举类) 列出条件变量上带超时等待的可能结果 { timeout, no_timeout}

虚假唤醒（Spurious Wakeup）：
    线程从 cv.wait(lk) 被唤醒，但并没有真正收到 notify，条件可能仍然不满足。
    不是 notify 触发的，条件仍然可能为 false，必须循环检查条件来保证正确性
    这是由于 操作系统调度或者底层实现机制（如 pthreads）可能无缘无故唤醒等待队列里的线程
    这是合法的行为，不是 bug，需要循环检查条件解决这个问题

丢失唤醒（Lost Wakeup）：
    线程在等待条件前或条件发生时，没有收到通知，导致线程永远被阻塞。
    notify 发出的时候，没有线程在等待，但是这时已经有线程判定条件不通过准备阻塞了，也就是没有互斥的访问条件；或者在获取锁和挂起线程之间发生竞争，信号被“丢掉”
    防止方式：
        必须用 mutex 保护条件，保证 wait 之前的条件判断和 notify之前的条件修改 互斥进行，一起使用同一把锁

    每个condition_variable对象cv底层绑定了一个等待线程队列（没有绑定判断条件本身）
    当有线程调用wait函数时，将这个线程挂起到等待线程队列中
    当有线程调用notify函数时，将挂起队列中的线程唤醒
    为了避免lost wakeup（丢失唤醒），即wait线程先等待条件发生，在阻塞之前调度到notify线程发出信号，然后wait线程才阻塞，唤醒通知发出在线程调用wait之前发生，导致wait线程错过唤醒永久睡死
    需要保证阻塞信号的访问和修改是互斥的，因此需要使用unique_lock保护线程。
    先获取锁，保证信号量中的条件的判断和修改是互斥的，然后如果条件不满足，释放锁，阻塞当前线程，挂起到挂起队列中
    当另一个线程需要唤醒别的线程时，先获取同一个mutex，保证互斥修改，然后修改判断条件，调用notify唤醒别的线程。
    线程被唤醒后会获取锁，这保证了阻塞队列中最多有一个线程被唤醒。当前线程在被 cv.wait(lk) 唤醒时，一定会重新获取 mtx。
    cv.notify_all() 唤醒所有等待线程，但每个线程仍然必须抢 mutex；mutex 的阻塞队列保证每个线程最终能继续执行，因此不会有线程“卡死”，只是按顺序进入临界区。
*/

using namespace std;

mutex mtx;
bool pred;
condition_variable cv;

void unique_wait() {
	unique_lock<mutex> lk(mtx);
	while(!pred) cv.wait(lk);
	cout << "unique_wait " << lk.owns_lock() << endl;
	// 这里的输出是永远不会因多线程重叠错位的，因为线程被唤醒后会自动获取锁的所有权，成功获取到mtx所有权的线程可以继续运行
	// 而没有获取到锁的线程虽然因为notify_all不在cv的阻塞队列里了，但是仍然在运行lk.lock()时被阻塞，只有锁释放时才会继续运行抢占锁
}

void unique_notify() {
	{
		lock_guard<mutex> lk(mtx); // 这里获取锁是为了防止唤醒错过，而notify_all最好不要在临界区内
		pred = true;
	} // 先unlock然后notify_all，减少虚假唤醒，即虽然被通知唤醒的线程不在cv的阻塞队列里了，但是会由于尝试获取mtx的所有权而阻塞
	// 如果先 notify 后 unlock，等待线程可能马上尝试获取锁，但锁还没释放，会浪费 CPU 进行竞争
	cv.notify_all(); // 不能将notify_all换成notify_one，这样只有一个线程被唤醒
}

shared_mutex smtx;
bool spred;
condition_variable_any scv;

void shared_wait() {
	shared_lock<shared_mutex> lk(
	    smtx); // 这里使用的是读锁，后续条件变量唤醒后会并行运行，通过 shared_lock 将 lock 的语义转化为 shared_lock
	while(!spred) scv.wait(lk); // wait 结束后线程获取到的是读锁，多个 shared_wait 线程可以并行运行
	cout << "shared_wait " << lk.owns_lock() << endl; // 这里是并行运行，会重叠
}

void shared_notify() {
	{
		lock_guard<shared_mutex> lk(smtx); // 这里使用写锁，因为需要修改 spred 的值
		spred = true;
	} // 先unlock然后notify_all，减少虚假唤醒，即虽然被通知唤醒的线程不在cv的阻塞队列里了，但是会由于尝试获取mtx的所有权而阻塞
	// 如果先 notify 后 unlock，等待线程可能马上尝试获取锁，但锁还没释放，会浪费 CPU 进行竞争
	scv.notify_all(); // 不能将notify_all换成notify_one，这样只有一个线程被唤醒
}

int main() {
	for(int i = 0; i < 100; ++i) {
		spred = false; // 放到jthread前，避免影响正常的执行流程
		jthread tw1(unique_wait);
		jthread tw2(unique_wait);
		jthread tw3(unique_wait);
		jthread tn(unique_notify);
	} // jthread是thread的RAII实现，在析构时自动 join
	// 运行结果是输出300行"unique_wait 1"，且不会错位和重叠

	this_thread::sleep_for(100ms);
	for(int i = 0; i < 100; ++i) {
		spred = false;
		jthread tw1(shared_wait);
		jthread tw2(shared_wait);
		jthread tw3(shared_wait);
		jthread tn(shared_notify);
	}
	return 0;
}