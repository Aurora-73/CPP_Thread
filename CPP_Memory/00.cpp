// 构造函数与析构函数
#include <iostream>
#include <string>
#include <utility>

/*
【构造函数（Constructor）】
  构造函数是特殊成员函数，用于初始化对象。
  调用时机：
    局部变量声明时
    全局/静态变量初始化时
    new 表达式执行时
    作为成员变量或基类被初始化时
    作为函数参数或返回值时（可能被优化，NRVO，RVO）

  构造函数种类：
    1. 默认构造函数（Default Constructor）
       无参数的构造函数，如果没有自定义任何构造函数，编译器会自动生成。
       作用：初始化对象的所有成员变量。
       注意：如果定义了任何带参数的构造函数，编译器不会自动生成默认构造函数。

    2. 带参数的构造函数（Parameterized Constructor）
       接收一个或多个参数用于初始化成员变量。
       编译器进行隐式类型转换时可能调用单参数构造函数。

    3. 拷贝构造函数（Copy Constructor）
       签名：ClassName(const ClassName& other);
       用途：从另一个相同类型的对象创建新对象，执行深拷贝。
       调用时机：
         对象作为函数参数按值传递时
         对象作为函数返回值时（C++17 强制复制消除）
         初始化新对象时 ClassName obj2 = obj1;
       如果未定义，编译器自动生成浅拷贝版本。

    4. 移动构造函数（Move Constructor）
       签名：ClassName(ClassName&& other) noexcept;
       用途：从将亡值（临时对象）"窃取"资源，而不是复制。
       调用时机：
         对象作为右值传入函数时
         返回局部对象时（与返回值优化结合）
         std::move() 转换为右值时
       通常标记为 noexcept，告知编译器不会抛异常。

    5. 显式转换构造函数（Explicit Constructor）
       explicit ClassName(OtherType value);
       防止隐式类型转换，必须显式调用。
       常见于单参数构造函数。

    6. 委托构造函数（Delegating Constructor）
       一个构造函数调用同一类的另一个构造函数来完成部分初始化。
       减少代码重复，集中初始化逻辑。

    7. 成员初始化列表（Member Initializer List）
       格式：ClassName(args) : member1(val1), member2(val2) {}
       作用：在进入构造函数体之前初始化成员变量。
       优点：
         效率高，直接初始化而非赋值
         对 const 成员和引用成员是必需的
         初始化顺序按成员声明顺序进行

【析构函数（Destructor）】
  析构函数是特殊成员函数，用于清理对象资源。
  语法：~ClassName() {}
  
  调用时机：
    局部变量作用域结束时
    全局/静态变量程序结束时
    动态分配对象使用 delete 时
    临时对象表达式结束时
    容器元素被删除或容器销毁时

  析构函数特点：
    不能有参数，不能被重载
    不能有返回值
    应该标记为 noexcept（通常不抛异常）
    在继承树中，基类析构函数应声明为 virtual

  RAII 模式（Resource Acquisition Is Initialization）：
    资源在构造函数中获取，在析构函数中释放。
    这是 C++ 资源管理的最佳实践。

  析构顺序：
    成员变量按声明的逆序析构
    如果有基类，先析构派生类，再析构基类
    多重继承时，从左到右的逆序析构

  move 不等于对象消失
    TestClass a;
    TestClass b(std::move(a));
    执行之后：a: moved-from 状态，b: 新对象
    但是 a 仍然是一个完整的 C++ 对象。
    它有对象地址、vptr（如果有）、成员变量、生命周期
    所以必须：a.~TestClass(); 否则：对象生命周期不完整

【特殊成员函数的自动生成】
  如果不显式定义以下特殊成员函数，编译器会自动生成：
    默认构造函数
    拷贝构造函数
    拷贝赋值操作符
    移动构造函数（C++11，如果没有自定义拷贝/移动/析构）
    移动赋值操作符（C++11，如果没有自定义拷贝/移动/析构）
    析构函数

  显式删除特殊成员函数：
    ClassName(const ClassName&) = delete;  // 禁止拷贝
    ClassName& operator=(const ClassName&) = delete;
    ClassName(ClassName&&) = delete;  // 禁止移动
*/

// 1：最基础的构造和析构
class Example1 {
	int value;

public:
	// 默认构造函数
	Example1() : value(0) {
		std::cout << "【Example1】 默认构造函数调用" << std::endl;
	}

	// 析构函数
	~Example1() noexcept {
		std::cout << "【Example1】 析构函数调用，value = " << value << std::endl;
	}
};

// 2：带参数的构造函数
class Example2 {
	int value;
	std::string name;

public:
	// 默认构造函数
	Example2() : value(0), name("default") {
		std::cout << "【Example2】 默认构造函数调用" << std::endl;
	}

	// 带参数的构造函数
	Example2(int v, const std::string &n) : value(v), name(n) {
		std::cout << "【Example2】 参数构造函数调用: value=" << value << ", name=" << name << std::endl;
	}

	~Example2() noexcept {
		std::cout << "【Example2】 析构函数调用，value=" << value << ", name=" << name << std::endl;
	}
};

// 3：拷贝构造函数
class Example3 {
	int *data;
	int size;

public:
	Example3() : data(nullptr), size(0) {
		std::cout << "【Example3】 默认构造函数调用" << std::endl;
	}

	Example3(int sz) : data(new int[sz]), size(sz) {
		std::cout << "【Example3】 参数构造函数调用，分配 " << sz << " 个元素" << std::endl;
		for(int i = 0; i < size; ++i) {
			data[i] = i;
		}
	}

	// 拷贝构造函数 执行深拷贝
	Example3(const Example3 &other) : data(new int[other.size]), size(other.size) {
		std::cout << "【Example3】 拷贝构造函数调用，复制 " << size << " 个元素" << std::endl;
		for(int i = 0; i < size; ++i) {
			data[i] = other.data[i];
		}
	}

	~Example3() noexcept {
		std::cout << "【Example3】 析构函数调用，释放 " << size << " 个元素" << std::endl;
		delete[] data;
	}
};

// 4：移动构造函数
class Example4 {
	int *data;
	int size;

public:
	Example4() : data(nullptr), size(0) {
		std::cout << "【Example4】 默认构造函数调用" << std::endl;
	}

	Example4(int sz) : data(new int[sz]), size(sz) {
		std::cout << "【Example4】 参数构造函数调用，分配 " << sz << " 个元素" << std::endl;
		for(int i = 0; i < size; ++i) {
			data[i] = i;
		}
	}

	// 拷贝构造函数
	Example4(const Example4 &other) : data(new int[other.size]), size(other.size) {
		std::cout << "【Example4】 拷贝构造函数调用" << std::endl;
		for(int i = 0; i < size; ++i) {
			data[i] = other.data[i];
		}
	}

	// 移动构造函数 "窃取"资源
	Example4(Example4 &&other) noexcept : data(other.data), size(other.size) {
		std::cout << "【Example4】 移动构造函数调用，窃取 " << size << " 个元素" << std::endl;
		other.data = nullptr;
		other.size = 0;
	}

	~Example4() noexcept {
		std::cout << "【Example4】 析构函数调用，释放 " << size << " 个元素";
		if(data == nullptr) {
			std::cout << "（已被移动，无需释放）" << std::endl;
		} else {
			std::cout << std::endl;
			delete[] data;
		}
	}
};

// 5：explicit 构造函数
class Example5 {
	int value;

public:
	// 显式构造函数，防止隐式转换
	explicit Example5(int v) : value(v) {
		std::cout << "【Example5】 explicit 构造函数调用，value=" << value << std::endl;
	}

	~Example5() noexcept {
		std::cout << "【Example5】 析构函数调用" << std::endl;
	}
};

// 6：委托构造函数
class Example6 {
	int x, y;

public:
	// 默认构造函数 委托给带参数的构造函数
	Example6() : Example6(0, 0) {
		std::cout << "【Example6】 默认构造函数（委托后）" << std::endl;
	}

	// 单参数构造函数 委托给双参数构造函数
	Example6(int val) : Example6(val, val) {
		std::cout << "【Example6】 单参数构造函数（委托后），val=" << val << std::endl;
	}

	// 双参数构造函数 执行实际初始化
	Example6(int a, int b) : x(a), y(b) {
		std::cout << "【Example6】 双参数构造函数（实际初始化），x=" << x << ", y=" << y << std::endl;
	}

	~Example6() noexcept {
		std::cout << "【Example6】 析构函数调用，x=" << x << ", y=" << y << std::endl;
	}
};

// 7：虚析构函数（继承场景）
class Base {
public:
	Base() {
		std::cout << "【Base】 构造函数调用" << std::endl;
	}

	// 虚析构函数 确保派生类析构函数被调用
	virtual ~Base() noexcept {
		std::cout << "【Base】 虚析构函数调用" << std::endl;
	}
};

class Derived : public Base {
	int *data;

public:
	Derived() : data(new int[10]) {
		std::cout << "【Derived】 构造函数调用，分配 10 个元素" << std::endl;
	}

	~Derived() noexcept override {
		std::cout << "【Derived】 析构函数调用，释放资源" << std::endl;
		delete[] data;
	}
};

int main() {
	std::cout << "==========  1：基础构造和析构 ==========" << std::endl;
	{ Example1 obj; } // 作用域结束，析构函数调用
	std::cout << std::endl;

	std::cout << "==========  2：带参数的构造函数 ==========" << std::endl;
	{
		Example2 obj1;             // 调用默认构造函数
		Example2 obj2(42, "test"); // 调用参数构造函数
	}
	std::cout << std::endl;

	std::cout << "==========  3：拷贝构造函数 ==========" << std::endl;
	{
		Example3 obj1(3);     // 参数构造函数
		Example3 obj2 = obj1; // 拷贝构造函数
		Example3 obj3(obj1);  // 拷贝构造函数（显式调用）
	}
	std::cout << std::endl;

	std::cout << "==========  4：移动构造函数 ==========" << std::endl;
	{
		Example4 obj1(5);                // 参数构造函数
		Example4 obj2 = std::move(obj1); // 移动构造函数
		                                 // obj1 的资源已被移走，析构时无需释放
	}
	std::cout << std::endl;

	std::cout << "==========  5：explicit 构造函数 ==========" << std::endl;
	{
		Example5 obj1(100); // 显式调用，OK
		                    // Example5 obj2 = 200;  // 编译错误：隐式转换被禁止
	}
	std::cout << std::endl;

	std::cout << "==========  6：委托构造函数 ==========" << std::endl;
	{
		Example6 obj1;         // 默认构造函数（委托给双参数）
		Example6 obj2(5);      // 单参数（委托给双参数）
		Example6 obj3(10, 20); // 双参数（直接初始化）
	}
	std::cout << std::endl;

	std::cout << "==========  7：虚析构函数 ==========" << std::endl;
	{
		Base *ptr = new Derived(); // 基类指针指向派生类对象
		delete ptr;                // 虚析构函数确保派生类析构被调用
	}
	std::cout << std::endl;

	std::cout << "==========  8：临时对象的构造和析构 ==========" << std::endl;
	{
		Example1().~Example1(); // 临时对象立即析构
	}
	std::cout << std::endl;

	std::cout << "==========  9：函数参数传递 ==========" << std::endl;
	{
		auto func = [](const Example1 &obj) {
			std::cout << "【func】 函数体执行" << std::endl;
		};

		std::cout << "【main】 创建对象并传入函数" << std::endl;
		Example1 obj;
		func(obj); // 按引用传递，无需拷贝构造
		std::cout << "【main】 函数返回" << std::endl;
	}
	std::cout << std::endl;

	return 0;
}
