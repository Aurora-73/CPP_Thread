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
    有两个关键成员（概念上）    mutex_type *pmutex;    bool owns;
    构造函数：
        explicit unique_lock( mutex_type& m )： pmutex = &m，对pmute加锁，owns = true
        unique_lock( mutex_type& m, std::defer_lock_t t )： pmutex = &m，owns = false，不加锁
        unique_lock( mutex_type& m, std::try_to_lock_t t )： pmutex = &m，owns = pmutex->try_lock()，尝试加锁
        unique_lock( mutex_type& m, std::adopt_lock_t t )： 接管已经加好的锁，pmutex = &m，owns = true，默认已经加锁，不再次加锁
    operator=
        如果拥有，则解锁当前互斥量，并获取另一个互斥量的所有权
    支持移动语义，不支持拷贝语义
    析构函数：
        if(owns) pmutex->unlock();
    lock：
        pmutex->lock(); owns = true;
    try_lock：
        owns = pmutex->try_lock();
    unlock：
        pmutex->unlock(); owns = false; 
    release：
        owns = false; auto __ret = pmutex; pmutex = nullptr; return __ret;
    mutex：
        return pmutex;
    owns_lock:
        return owns;
    operator bool:
        return owns;

shared_lock  实现可移动的共享互斥量所有权包装器 (类模板)

defer_lock 用于指定锁定策略的标签
try_to_lock
adopt_lock
defer_lock_t
try_to_lock_t
adopt_lock_t

*/

int main() {
	mutex mtx;
	cout << boolalpha;
	unique_lock<mutex> ul(mtx, defer_lock_t());
	cout << ul.mutex() << endl;
	cout << ul.owns_lock() << endl;
	ul.lock();
	cout << ul.owns_lock() << endl;
	ul.release();
	cout << ul.mutex() << endl;
	ul.lock();
	cout << ul.owns_lock() << endl;
}