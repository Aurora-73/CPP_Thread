#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

// 原子操作 与 atomic
/*  */

using namespace std;

void pause() {
	cout << "Press Enter to continue..." << endl;
	cin.get();
}
int main() {
	{ }

	pause();

	{ }

	return 0;
}