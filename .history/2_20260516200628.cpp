#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

using namespace std;


// 通用互斥锁管理（RAII）
/*
lock_guard  实现严格基于作用域的互斥体所有权包装器 (类模板)

    构造函数：
        explicit lock_guard( mutex_type& m ); 引用m为成员变量，并调用 m.lock()。
        lock_guard( mutex_type& m, std::adopt_lock_t t ); 引用m为成员变量，不调用 m.lock()。
    

scoped_lock  用于多个互斥体的死锁避免 RAII 包装器 (类模板)

unique_lock  实现可移动的互斥体所有权包装器 (类模板)

shared_lock  实现可移动的共享互斥量所有权包装器 (类模板)

defer_lock 用于指定锁定策略的标签
try_to_lock
adopt_lock
defer_lock_t
try_to_lock_t
adopt_lock_t

*/

int main() { 

}