#include <chrono>
#include <iostream>
#include <mutex>
#include <shared_mutex>
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
        对关联互斥量加锁；pmutex->lock(); owns = true; 若未关联或已持有锁，会抛异常
    try_lock：
        尝试加锁，成功则 owns = true，失败则 owns = false 即 owns = pmutex->try_lock();
    unlock：
        释放当前持有的锁；pmutex->unlock(); owns = false; 若未持有锁，会抛异常
    release：
        放弃所有权但不解锁，返回 pmutex，并将自身置为空
        owns = false; auto __ret = pmutex; pmutex = nullptr; return __ret;
    mutex：
        返回当前关联的 mutex 指针
    owns_lock：
        返回当前是否持有锁 即 return owns;
    operator bool：
        等价于 owns_lock()

shared_lock  实现可移动的共享互斥量所有权包装器 (类模板)
    类似于 unique_lock 只是lock相关的操作都变为 shared

defer_lock_t、try_to_lock_t、adopt_lock_t 是三个标签类
defer_lock、try_to_lock、adopt_lock 是三个标签类的 constexpr 对象，用于指定锁定策略的标签

辅助函数：
    template< class Lockable1, class Lockable2, class... LockableN >
    int try_lock ( Lockable1& lock1, Lockable2& lock2, LockableN&... lockn );
        尝试通过按顺序调用每个给定 Lockable 对象 lock1、lock2、...、lockn 的 try_lock 来锁定它们，从第一个开始。
        如果对 try_lock 的调用失败，则不会执行对 try_lock 的后续调用，将对任何已锁定的对象调用 unlock，并返回失败锁定对象的从 ​0​ 开始的索引。
        如果对 try_lock 的调用导致异常，则在重新抛出之前，会对任何已锁定的对象调用 unlock。
        成功时返回 -1，或返回失败锁定对象的从 ​0​ 开始的索引值。

    template< class Lockable1, class Lockable2, class... LockableN >
    void lock( Lockable1& lock1, Lockable2& lock2, LockableN&... lockn );
        使用死锁避免算法锁定给定的 Lockable 对象 lock1、lock2、...、lockn 以避免死锁。
        这些对象通过一系列未指定的 lock、try_lock 和 unlock 调用来锁定。如果对 lock 或 unlock 的调用导致异常，则在重新抛出之前，会对所有已锁定的对象调用 unlock。

    template< class Callable, class... Args >
    void call_once( std::once_flag& flag, Callable&& f, Args&&... args );
        即使在多个线程中同时调用，f这个可调用 (Callable) 对象也只会被精确地执行一次。
        如果，当调用std::call_once时，flag指示f已被调用过，std::call_once会立即返回（这种对std::call_once的调用被称为被动 (passive)）。
        否则，std::call_once会调用INVOKE(std::forward<Callable>(f), std::forward<Args>(args)...)。与std::thread构造函数或std::async不同，参数不会被移动或复制，因为它们不需要转移到另一个执行线程（这种对std::call_once的调用被称为主动 (active)）。
        如果该调用抛出异常，则该异常会传播到std::call_once的调用者，并且flag不会被翻转，以便再次尝试调用（这种对std::call_once的调用被称为异常 (exceptional)）。
        如果该调用正常返回（这种对std::call_once的调用被称为返回 (returning)），则flag会被翻转，并且所有其他使用相同flag的std::call_once调用都保证是被动 (passive)的。
        对相同flag的所有主动 (active)调用形成一个单一的总顺序，该顺序由零个或多个异常 (exceptional)调用，后跟一个返回 (returning)调用组成。每个主动 (active)调用的结束与该顺序中的下一个主动 (active)调用同步。
        返回 (returning)调用的返回与使用相同flag的所有被动 (passive)调用的返回同步：这意味着所有对std::call_once的并发调用都保证能够观察到主动 (active)调用所产生的任何副作用，而无需额外的同步。
    参数
    flag	-	一个对象，其关联的函数只会执行一次。
    f	-	要调用的可调用 (Callable) 对象。
    args...	-	要传递给函数的参数。
    返回值
    (无)

    异常
    如果任何条件阻止std::call_once按照指定执行，则抛出std::system_error。
    f抛出的任何异常。
*/

int main() { }