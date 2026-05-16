#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// 访问不同内存可以不上锁

using namespace std;
mutex mtx;

int main() { }
// t1 t2 t3分别访问不同的内存，不需要上锁
// 但是如果多一个进程修改vector的长度，则需要全部上锁