#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

using namespace std;

// 通用互斥锁管理（RAII）
/*
注意：C++中临时对象（未命名的变量）的生命周期截止到完整表达式结束！！！！

lock_guard  实现严格基于作用域的互斥体所有权包装器 (类模板)
    构造函数：
        explicit lock_guard( mutex_type& m ); 引用m为成员变量，并调用 m.lock()。
        lock_guard( mutex_type& m, std::adopt_lock_t t ); 引用m为成员变量，不调用 m.lock()。
        支持移动构造函数，不支持拷贝构造函数
    析构函数：
        释放所拥有互斥量的所有权。实际调用 m.unlock()。销毁构对象
    等价写法：
        template<class Mutex>
        class lock_guard {
            Mutex& m;
        public:
            explicit lock_guard(Mutex& m_) : m(m_) { m.lock(); }
            ~lock_guard() { m.unlock(); }
        };

scoped_lock  用于多个互斥体的死锁避免 RAII 包装器 (类模板)
    创建 scoped_lock 对象时，它会尝试获取它所给定的互斥体的所有权。
    当控制离开创建 scoped_lock 对象的范围时，scoped_lock 会被销毁，并且互斥体会被释放。
    如果给定多个互斥体，则会使用死锁避免算法，如同通过 std::lock

unique_lock  实现可移动的互斥体所有权包装器 (类模板)
    有两个关键成员（概念上）： mutex_type *pmutex; bool owns;
    构造函数：
        unique_lock()： pmutex = nullptr，owns = false
        explicit unique_lock( mutex_type& m )： 关联 m 并立即加锁，owns = true
        unique_lock( mutex_type& m, std::defer_lock_t )： 关联 m，但不加锁，owns = false
        unique_lock( mutex_type& m, std::try_to_lock_t )： 关联 m，并尝试加锁，owns = pmutex->try_lock()
        unique_lock( mutex_type& m, std::adopt_lock_t )： 接管一个已经持有的锁，owns = true
        unique_lock( mutex_type& m, const chrono::time_point<...>& ) / const chrono::duration<...>&： 超时尝试加锁
    支持移动构造和移动赋值，不支持拷贝；移动后原对象变为空壳（pmutex = nullptr, owns = false）
    operator=：
        通过移动转移所有权；若当前对象持有锁，会先释放原锁，再接管新对象的状态
    析构函数：
        if(owns) pmutex->unlock();
    lock：
        对关联互斥量加锁；若未关联或已持有锁，会抛异常
    try_lock：
        尝试加锁，成功则 owns = true，失败则 owns = false
    unlock：
        释放当前持有的锁；若未持有锁，会抛异常
    release：
        放弃所有权但不解锁，返回 pmutex，并将自身置为空
    mutex：
        返回当前关联的 mutex 指针
    owns_lock：
        返回当前是否持有锁
    operator bool：
        等价于 owns_lock()

shared_lock  实现可移动的共享互斥量所有权包装器 (类模板)

defer_lock 用于指定锁定策略的标签
try_to_lock
adopt_lock
defer_lock_t
try_to_lock_t
adopt_lock_t

*/

int main() {
	shared_mutex mtx;
	cout << boolalpha;
	shared_lock<mutex> ul(mtx);
	cout << ul.mutex() << endl;
	cout << ul.owns_lock() << endl;
	ul.lock();
	cout << ul.owns_lock() << endl;
	ul.release();
	cout << ul.mutex() << endl;
	ul.lock();
	cout << ul.owns_lock() << endl;
}