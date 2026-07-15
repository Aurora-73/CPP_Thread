// new 和 delete
#include <cstdlib> // malloc/free
#include <iostream>
#include <new> // std::bad_alloc
#include <stdexcept>

/*
【new / delete 表达式】
  new 表达式包含两个步骤：
    1. 调用 operator new 分配足够的原始内存；
    2. 在该内存上调用对象构造函数。

  如果构造函数抛出异常，编译器会自动调用对应的 operator delete 释放已经分配的内存，因此不会发生内存泄漏。

  new[] 用于创建对象数组：
    1. 调用 operator new[] 分配整个数组所需内存；
    2. 依次调用每个元素的构造函数。
    实现通常会额外保存数组元素数量等信息（array cookie），用于 delete[] 时正确调用析构函数。

  delete 表达式同样包含两个步骤：
    1. 调用对象析构函数；
    2. 调用 operator delete 释放内存。

  delete[] 用于释放对象数组：
    1. 按逆序依次调用每个元素的析构函数；（析构顺序与构造顺序相反，符合对象生命周期规则。）
    2. 调用 operator delete[] 释放整块内存。

	delete 必须与 new 配对，delete[] 必须与 new[] 配对，两者不能混用。

	另外要注意：
		析构函数需要声明为 virtual，
		否则通过基类指针 delete 派生类对象可能导致未定义行为。

  new 失败怎么办？
    普通 new：Test* p = new Test;
        失败时 throw std::bad_alloc;
    nothrow：Test* p = new(std::nothrow) Test;
        失败： p == nullptr

    operator delete 不可以是 virtual 因为它是 static 成员函数

【operator new / operator delete】
  operator new/operator delete 是负责原始内存分配和释放的函数；
  new/delete 是 C++ 语言级表达式，负责对象生命周期管理。

	可以重载 operator new / delete 来实现自定义内存管理。
	operator new / operator delete 存在全局作用域和类内两个版本，
	类内 operator new/delete 本质上是静态成员函数语义，声明时通常不写 static。

	实际实现 operator new 时还要保证：
		1. 分配失败时抛出 std::bad_alloc；
		2. 返回满足 C++ 对齐要求的内存。

【placement new】
    placement new 是一个透传函数，用于在特定的内存上构造对象，相当于在这个位置调用构造函数。
	它不负责分配内存，只负责“在给定地址上构造对象”。
	与手动调用的析构函数配套使用 p->~T();
	构造成功后不要对这块内存再调用 delete；
	需要先手动析构对象，再由外部释放原始缓冲区。

	标准 placement new 不让重载，等价于 
    void* operator new(std::size_t, void* ptr) noexcept {
        return ptr;
    }

	常见用途：
		1. 在预分配的原始缓冲区里构造对象；
		2. 对象重建（先析构，再在原地重新构造）；
		3. 自己管理对象生命周期的内存池/容器实现。

【普通 delete 与 sized delete】
  C++14 引入了 sized delete：
    void operator delete(void* p);
    void operator delete(void* p, std::size_t size);

  普通 delete：
    编译器只传递指针。

	sized delete：
		编译器传递静态类型对应的对象大小。
		自定义内存分配器可以利用 size 提高释放效率，
		避免再次查询对象大小。

		这里的 size 不一定等于动态类型的 sizeof(Derived)，
		更准确地说，通常是 delete 表达式静态类型对应的大小。

  是否调用 sized delete 由编译器决定，
  用户仍然写 delete p;，不会直接调用它。

【全局作用域】
  全局作用域（global scope）是：
  不属于任何函数、类、命名空间的最外层作用域。

  :: 为全局作用域解析符，例如：
    ::printf();
    ::std::cout;

【static 与匿名命名空间】
  文件作用域下：
    static int x;
    表示 x 仅在当前 cpp 文件可见（internal linkage）。

  匿名命名空间：
    作用也是限制符号仅在当前 cpp 文件可见，
    但还能包含类、模板、类型别名等，因此现代 C++
    更推荐使用匿名命名空间代替 static 修饰文件作用域对象。
*/

// 重载全局 new 运算符
void *operator new(std::size_t size) {
	std::cout << "Custom new operator called. Size: " << size << std::endl;
	void *mem = malloc(size);
	if(!mem) throw std::bad_alloc();
	return mem;
}

// 重载全局 new[] 运算符
void *operator new[](std::size_t size) {
	std::cout << "Custom new[] operator called. Size: " << size << std::endl;
	void *mem = malloc(size);
	if(!mem) throw std::bad_alloc();
	return mem;
}

// 重载全局 nothrow new 运算符
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
	std::cout << "Custom nothrow new operator called. Size: " << size << std::endl;
	return malloc(size);
}

// 重载全局 nothrow new[] 运算符
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
	std::cout << "Custom nothrow new[] operator called. Size: " << size << std::endl;
	return malloc(size);
}

// 重载全局 delete 运算符
void operator delete(void *ptr) noexcept { // 需要声明 noexcept
	std::cout << "Custom delete operator called." << std::endl;
	free(ptr);
}

// 重载全局 sized delete 运算符
void operator delete(void *ptr, std::size_t size) noexcept {
	std::cout << "Custom sized delete operator called. Size: " << size << std::endl;
	free(ptr);
}

// 重载全局 delete[] 运算符
void operator delete[](void *ptr) noexcept {
	std::cout << "Custom delete[] operator called." << std::endl;
	free(ptr);
}

// 重载全局 sized delete[] 运算符
void operator delete[](void *ptr, std::size_t size) noexcept {
	std::cout << "Custom sized delete[] operator called. Size: " << size << std::endl;
	free(ptr);
}

// nothrow new 的配套释放函数：仅在 new(std::nothrow) 分配成功但构造阶段抛异常时使用。
// 构造成功后执行 delete p; 仍然走普通 operator delete(void*)，不会走这里。
void operator delete(void *ptr, const std::nothrow_t &) noexcept {
	std::cout << "Custom nothrow delete operator called." << std::endl;
	free(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept {
	std::cout << "Custom nothrow delete[] operator called." << std::endl;
	free(ptr);
}

class TestClass1 {
public:
	// 测试点1：普通对象的构造/析构，只用于观察栈上对象生命周期
	TestClass1() {
		std::cout << "TestClass constructor called." << std::endl;
	}
	~TestClass1() noexcept {
		std::cout << "TestClass destructor called." << std::endl;
	}
};

class TestClass2 {
public:
	// 测试点2：类内重载 operator new / delete，观察成员版优先级
	TestClass2() {
		std::cout << "TestClass2 constructor called." << std::endl;
	}

	~TestClass2() noexcept {
		std::cout << "TestClass2 destructor called." << std::endl;
	}
	// 重载类的 new 运算符
	void *operator new(std::size_t size) {
		std::cout << "Class new operator called. Size: " << size << std::endl;
		void *mem = malloc(size);
		if(!mem) throw std::bad_alloc();
		return mem;
	}
	// 重载类的 new[] 运算符
	void *operator new[](std::size_t size) {
		std::cout << "Class new[] operator called. Size: " << size << std::endl;
		void *mem = malloc(size);
		if(!mem) throw std::bad_alloc();
		return mem;
	}
	// 重载类的 nothrow new 运算符
	void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
		std::cout << "Class nothrow new operator called. Size: " << size << std::endl;
		return malloc(size);
	}
	// 重载类的 nothrow new[] 运算符
	void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
		std::cout << "Class nothrow new[] operator called. Size: " << size << std::endl;
		return malloc(size);
	}
	// 重载类的 delete 运算符
	void operator delete(void *ptr) noexcept {
		std::cout << "Class delete operator called." << std::endl;
		free(ptr);
	}
	// 重载类的 sized delete 运算符
	void operator delete(void *ptr, std::size_t size) noexcept {
		std::cout << "Class sized delete operator called. Size: " << size << std::endl;
		free(ptr);
	}
	// 重载类的 delete[] 运算符
	void operator delete[](void *ptr) noexcept {
		std::cout << "Custom delete[] operator called." << std::endl;
		free(ptr);
	}
	// 重载类的 sized delete[] 运算符
	void operator delete[](void *ptr, std::size_t size) noexcept {
		std::cout << "Custom sized delete[] operator called. Size: " << size << std::endl;
		free(ptr);
	}
	// nothrow new 的配套释放函数
	void operator delete(void *ptr, const std::nothrow_t &) noexcept {
		std::cout << "Class nothrow delete operator called." << std::endl;
		free(ptr);
	}
	void operator delete[](void *ptr, const std::nothrow_t &) noexcept {
		std::cout << "Class nothrow delete[] operator called." << std::endl;
		free(ptr);
	}
};

class ThrowOnConstruct {
public:
	// 测试点3：构造函数主动抛异常，用来 nothrow new 的异常行为
	ThrowOnConstruct() {
		std::cout << "ThrowOnConstruct constructor called." << std::endl;
		throw std::runtime_error("constructor failure");
	}

	~ThrowOnConstruct() noexcept {
		std::cout << "ThrowOnConstruct destructor called." << std::endl;
	}
};
class ThrowOnConstructNormal {
public:
	ThrowOnConstructNormal() {
		std::cout << "ThrowOnConstructNormal constructor called." << std::endl;
		throw std::runtime_error("normal constructor failure");
	}

	~ThrowOnConstructNormal() noexcept {
		std::cout << "ThrowOnConstructNormal destructor called." << std::endl;
	}
};

class PlacementDemo {
public:
	PlacementDemo() {
		std::cout << "PlacementDemo constructor called." << std::endl;
	}

	~PlacementDemo() noexcept {
		std::cout << "PlacementDemo destructor called." << std::endl;
	}
};

int main() {
	// 测试 栈上普通对象，确认构造和析构顺序正常
	std::cout << "----------------------------" << std::endl;
	{
		std::cout << "栈区定义" << std::endl;
		TestClass1 tc1;
		std::cout << "栈区析构" << std::endl;
	}

	// 测试 全局版 new/delete，用单个对象分配与释放路径
	std::cout << "----------------------------" << std::endl;
	std::cout << "调用全局运算符" << std::endl;
	std::cout << "堆区定义" << std::endl;
	TestClass1 *ptr1 = new TestClass1;
	std::cout << "堆区析构" << std::endl;
	delete ptr1;

	// 测试 栈上类对象，确认 TestClass2 仍然只是普通栈对象
	std::cout << "----------------------------" << std::endl;
	{
		std::cout << "栈区定义" << std::endl;
		TestClass2 tc2;
		std::cout << "栈区析构" << std::endl;
	}

	// 测试 类内 new/delete 单对象，类内重载会覆盖全局版本
	std::cout << "----------------------------" << std::endl;
	std::cout << "调用类内运算符" << std::endl;
	std::cout << "堆区定义" << std::endl;
	TestClass2 *ptr2 = new TestClass2;
	std::cout << "堆区析构" << std::endl;
	delete ptr2;

	// 测试 全局 nothrow new 单对象，“分配失败不抛异常”的接口形态
	std::cout << "----------------------------" << std::endl;
	std::cout << "nothrow 调用全局运算符" << std::endl;
	std::cout << "堆区定义" << std::endl;
	TestClass1 *ptr1_nt = new(std::nothrow) TestClass1;
	if(ptr1_nt) {
		std::cout << "堆区析构" << std::endl;
		delete ptr1_nt;
	}

	// 测试 全局 nothrow new[]，数组版本也能走不抛异常的分配接口
	std::cout << "----------------------------" << std::endl;
	std::cout << "nothrow 调用全局运算符数组" << std::endl;
	std::cout << "堆区定义数组" << std::endl;
	TestClass1 *ptr_array1_nt = new(std::nothrow) TestClass1[3];
	if(ptr_array1_nt) {
		std::cout << "堆区析构数组" << std::endl;
		delete[] ptr_array1_nt;
	}

	// 测试 类内 nothrow new 单对象，成员版 nothrow 重载优先级
	std::cout << "----------------------------" << std::endl;
	std::cout << "nothrow 调用类内运算符" << std::endl;
	std::cout << "堆区定义" << std::endl;
	TestClass2 *ptr2_nt = new(std::nothrow) TestClass2;
	if(ptr2_nt) {
		std::cout << "堆区析构" << std::endl;
		delete ptr2_nt;
	}

	// 测试 类内 nothrow new[]，数组版本的成员重载是否生效
	std::cout << "----------------------------" << std::endl;
	std::cout << "nothrow 调用类内运算符数组" << std::endl;
	std::cout << "堆区定义数组" << std::endl;
	TestClass2 *ptr_array2_nt = new(std::nothrow) TestClass2[3];
	if(ptr_array2_nt) {
		std::cout << "堆区析构数组" << std::endl;
		delete[] ptr_array2_nt;
	}

	// 测试 构造函数抛异常，观察 nothrow new 是否只影响分配阶段，不影响构造阶段异常
	std::cout << "----------------------------" << std::endl;
	std::cout << "nothrow 构造异常演示" << std::endl;
	try {
		ThrowOnConstruct *ptr_throw = new(std::nothrow) ThrowOnConstruct;
		if(ptr_throw == nullptr) {
			std::cout << "nothrow 返回空指针" << std::endl;
		}
	} catch(const std::exception &e) {
		std::cout << "Caught exception: " << e.what() << std::endl;
	}

	// 测试K：placement new，验证“在指定原始内存上构造对象”
	// 这里先准备一块对齐到 PlacementDemo 的原始缓冲区，再在同一地址上构造对象。
	std::cout << "----------------------------" << std::endl;
	std::cout << "placement new 演示" << std::endl;
	alignas(PlacementDemo) unsigned char placement_buffer[sizeof(PlacementDemo)]; // 栈区 buffer
	std::cout << "原始缓冲区地址: " << static_cast<void *>(placement_buffer) << std::endl;

	PlacementDemo *placement_ptr = new(placement_buffer) PlacementDemo;
	std::cout << "placement new 得到的对象地址: " << static_cast<void *>(placement_ptr) << std::endl;

	// placement new 不对应 delete；这里必须手动调用析构函数。
	placement_ptr->~PlacementDemo();
	std::cout << "placement 对象已手动析构，缓冲区仍然保留，可供后续复用" << std::endl;

	// 测试J：普通 new 构造异常，验证“先分配内存，再构造；构造失败自动释放内存”
	std::cout << "----------------------------" << std::endl;
	std::cout << "普通 new 构造异常演示" << std::endl;
	try {
		ThrowOnConstructNormal *ptr_throw_normal = new ThrowOnConstructNormal;
		(void)ptr_throw_normal;
	} catch(const std::exception &e) {
		std::cout << "Caught exception: " << e.what() << std::endl;
	}

	// 测试 栈上数组对象，观察数组构造/析构顺序
	std::cout << "----------------------------" << std::endl;

	{
		std::cout << "栈区定义数组" << std::endl;
		TestClass1 tc1[3];
		std::cout << "栈区析构数组" << std::endl;
	}

	// 测试 全局 new[] / delete[] 单纯数组版本，数组分配和 array cookie 行为
	std::cout << "----------------------------" << std::endl;
	std::cout << "调用全局运算符" << std::endl;
	std::cout << "堆区定义数组" << std::endl;
	TestClass1 *ptr_array1 = new TestClass1[3];
	std::cout << "堆区析构数组" << std::endl;
	delete[] ptr_array1;

	// 测试 栈上类数组对象，确认类数组对象仍然只是普通栈生命周期
	std::cout << "----------------------------" << std::endl;
	{
		std::cout << "栈区定义数组" << std::endl;
		TestClass2 tc2[3];
		std::cout << "栈区析构数组" << std::endl;
	}

	// 测试 类内 new[] / delete[] 数组版本，成员数组重载路径
	std::cout << "----------------------------" << std::endl;
	std::cout << "调用类内运算符" << std::endl;
	std::cout << "堆区定义数组" << std::endl;
	TestClass2 *ptr_array2 = new TestClass2[3];
	std::cout << "堆区析构数组" << std::endl;
	delete[] ptr_array2;

	std::cout << "----------------------------" << std::endl;
}