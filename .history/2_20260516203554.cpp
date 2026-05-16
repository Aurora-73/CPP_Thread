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
    有两个关键成员（概念上）    mutex_type &pmutex;    bool owns;
    构造函数：
        explicit unique_lock( mutex_type& m ); 用pmutex引用m，对pmute加锁，owns设置true
        unique_lock( mutex_type& m, std::defer_lock_t t ); 用pmutex引用m，owns设置false，不加锁
        unique_lock( mutex_type& m, std::try_to_lock_t t ); 用pmutex引用m，owns设为pmutex.try_to_lock()的调用返回值
    析构函数：
        如果拥有，则解锁（即释放）关联的互斥量

    operator=
        如果拥有，则解锁（即释放）互斥量，并获取另一个互斥量的所有权
    (public member function)
    加锁
    lock
    
    锁定（即获取）关联的互斥量
    (public member function)
    try_lock
    
    尝试锁定（即获取）关联的互斥量而不阻塞
    (public member function)
    try_lock_for
    
    尝试锁定（即获取）关联的 TimedLockable 互斥量，如果在指定持续时间内互斥量不可用则返回
    (public member function)
    try_lock_until
    
    尝试锁定（即获取）关联的 TimedLockable 互斥量，如果在指定时间点之前互斥量不可用则返回
    (public member function)
    unlock
    
    解锁（即释放）关联的互斥量
    (public member function)
    修改器
    swap
    
    与另一个 std::unique_lock 交换状态
    (public member function)
    release
    
    解除与关联互斥量的关联，而不解锁（即释放）它
    (public member function)
    观察器
    mutex
    
    返回指向关联互斥量的指针
    (public member function)
    owns_lock
    
    测试锁是否拥有（即已锁定）其关联的互斥量
    (public member function)
    operator bool
    
    测试锁是否拥有（即已锁定）其关联的互斥量
    (public member function)
    非成员函数
    std::swap(std::unique_lock)
    
    (C++11)
    
    特化 std::swap 算法
    (function template)

shared_lock  实现可移动的共享互斥量所有权包装器 (类模板)

defer_lock 用于指定锁定策略的标签
try_to_lock
adopt_lock
defer_lock_t
try_to_lock_t
adopt_lock_t

*/

int main() { }