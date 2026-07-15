// 自定义 allocator
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

/*
【自定义 allocator】

    不推荐直接特例化 std::allocator，修改 std 命名空间很危险。
    正确方式是自己实现一个 allocator，然后传递给 STL 容器 std::vector<T, MyAllocator<T>>

一、allocator 的基本职责
    allocator 负责：分配和释放原始内存 \ 提供类型信息
    allocator 不负责：对象构造 \ 对象析构
    allocate() 未初始化内存
    construct_at() 对象生命周期开始
    destroy_at() 对象生命周期结束
    deallocate() 释放原始内存

二、最基本需要提供的接口，最小 allocator：
    template<typename T>
    class MyAllocator {
    public:
        using value_type = T; // 表示该 allocator 分配的对象类型。
        T* allocate(size_t n); // 分配 n 个 T 对象需要的原始内存。不调用构造函数。
        void deallocate(T* p, size_t n); // 释放 allocate 得到的内存。不调用析构函数。
    };

三、allocator_traits
    STL 容器不会直接使用 allocator，
    而是通过 std::allocator_traits<Allocator> 访问 allocator。
    allocator_traits 会自动补全很多信息
    通常不需要自己实现 allocator_traits。

四、推荐提供的类型成员
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    这些不是全部必须，但是可以提高兼容性。

五、rebind：重新绑定 allocator 类型
    一个 allocator 通常只为一种 T 分配内存 MyAllocator<int>，但是 STL 容器内部经常需要其他类型。
    例如 std::list<int, MyAllocator<int>> list 实际存储 struct Node; 因此需要 MyAllocator<int>  ->  MyAllocator<Node>

    C++11 以前要求 allocator 提供：
        template<typename U>
        struct rebind
        {
            using other = MyAllocator<U>;
        };

    现代 C++ 中：
        allocator_traits 会自动推导 allocator_traits<Alloc>::rebind_alloc<U>
    因此 rebind 通常可以省略。

六、为什么需要转换构造函数？
    rebind 只能解决类型转换，但是 STL 还需要创建对应 allocator 对象 MyAllocator<Node> node_alloc(alloc);
    因此需要 template<typename U> MyAllocator(const MyAllocator<U>& other);
    它负责将 allocator<int> 对象 转换为 allocator<Node> 对象
    如果 allocator 是无状态的，MyAllocator<int> 和 MyAllocator<Node> 完全一样，构造函数可以为空。
    如果 allocator 有状态，那么转换时必须保留状态。
        例如：class PoolAllocator
            {
                MemoryPool* pool;
            };
            需要 template<typename U> PoolAllocator(const PoolAllocator<U>& other) : pool(other.pool) {}
            否则：原 allocator 使用 pool A 转换后的 allocator 可能没有 pool 或使用错误 pool。

    实际上的流程是 
        第一步：using NodeAlloc = allocator_traits<Alloc>::rebind_alloc<Node>;
        第二步构造：NodeAlloc node_alloc(alloc); 调用转换构造函数 

七、operator== 和 is_always_equal
    allocator 需要告诉 STL 不同 allocator 实例是否等价。
    例如：
        无状态 allocator：
            MyAllocator<int>
            MyAllocator<double>
            都使用 ::operator new
                ::operator delete
            所以 using is_always_equal = std::true_type;
            表示任何实例都可以释放其他实例分配的内存。
        而 PoolAllocator<int>(pool1) 和 PoolAllocator<int>(pool2) 不同则 is_always_equal = false。

八、construct/destroy
    C++17 以前 allocator 通常提供：
        template<class U, class... Args> void construct(U* p, Args&&... args); 调用 placement new
        template<class U> void destroy(U* p); 显式调用析构函数
    C++20 后：
        STL 更倾向使用 std::allocator_traits，以及 std::construct_at() 和 std::destroy_at()
*/

template <typename T>
class MyAllocator {
public:
	using value_type = T;
	using pointer = T *;
	using const_pointer = const T *;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using is_always_equal = std::true_type; // 表示 MyAllocator<T> 实例永远等价

	constexpr MyAllocator() noexcept { }

	template <typename U>
	constexpr MyAllocator(const MyAllocator<U> &) noexcept { } // 转换构造函数，无状态直接返回默认的

	// allocate 分配 n 个 T 对象的裸内存
	[[nodiscard]] T *allocate(size_type n) {
		if(n > max_size()) {
			throw std::bad_array_new_length();
		}
		return static_cast<T *>(::operator new(n * sizeof(T)));
	}

	// 释放裸内存，不负责析构对象，需要先析构
	void deallocate(T *p, size_type) {
		::operator delete(p);
	}

	// 最大可分配数量
	constexpr size_type max_size() const noexcept {
		return std::numeric_limits<size_type>::max() / sizeof(T);
	}

	// 类型转换
	template <typename U>
	struct rebind {
		using other = MyAllocator<U>;
	};
};

// allocator 比较，无状态
template <typename T, typename U>
constexpr bool operator==(const MyAllocator<T> &, const MyAllocator<U> &) noexcept {
	return true;
}
template <typename T, typename U>
constexpr bool operator!=(const MyAllocator<T> &, const MyAllocator<U> &) noexcept {
	return false;
}

class TestClass {
	int a;

public:
	int get() const {
		return a;
	}

	TestClass() : a(0) {
		std::cout << "TestClass default constructor called." << std::endl;
	}

	TestClass(int val) : a(val) {
		std::cout << "TestClass constructor with param " << val << " called." << std::endl;
	}

	// 拷贝构造函数
	TestClass(const TestClass &other) : a(other.a) {
		std::cout << "TestClass copy constructor called." << std::endl;
	}

	// 拷贝赋值操作符
	TestClass &operator=(const TestClass &other) {
		std::cout << "TestClass copy assignment operator called." << std::endl;
		if(this != &other) {
			a = other.a;
		}
		return *this;
	}

	// 移动构造函数
	TestClass(TestClass &&other) noexcept : a(other.a) {
		std::cout << "TestClass move constructor called." << std::endl;
		other.a = 0; // 清零源对象
	}

	// 移动赋值操作符
	TestClass &operator=(TestClass &&other) noexcept {
		std::cout << "TestClass move assignment operator called." << std::endl;
		if(this != &other) {
			a = other.a;
			other.a = 0; // 清零源对象
		}
		return *this;
	}

	~TestClass() noexcept {
		std::cout << "TestClass destructor called." << std::endl;
	}
};

int main() {
	std::cout << "========== 1. 手动测试特殊成员函数 ==========" << std::endl;
	{
		TestClass a;
		TestClass b(10);
		TestClass c = b;
		TestClass d(std::move(b));
		a = c;
		a = std::move(d);
		std::cout << "a.get() = " << a.get() << std::endl;
	}
	std::cout << std::endl;

	std::cout << "========== 2. vector 的构造、拷贝、移动和扩容 ==========" << std::endl;
	MyAllocator<TestClass> alloc;
	std::vector<TestClass, MyAllocator<TestClass>> vec(alloc);

	std::cout << "reserve(2) 后连续 emplace_back" << std::endl;
	vec.reserve(2);
	vec.emplace_back();
	vec.emplace_back(1);

	std::cout << "push_back 左值（触发拷贝构造）" << std::endl;
	TestClass temp_copy(20);
	vec.push_back(temp_copy);

	std::cout << "push_back 右值（触发移动构造）" << std::endl;
	vec.push_back(TestClass(30));

	std::cout << "通过 resize 扩容观察已有元素搬移" << std::endl;
	vec.resize(8);

	std::cout << "vector 拷贝构造" << std::endl;
	std::vector<TestClass, MyAllocator<TestClass>> vec_copy(vec);

	std::cout << "vector 拷贝赋值" << std::endl;
	std::vector<TestClass, MyAllocator<TestClass>> vec_assigned(alloc);
	vec_assigned = vec_copy;

	std::cout << "vector 移动构造" << std::endl;
	std::vector<TestClass, MyAllocator<TestClass>> vec_moved(std::move(vec_assigned));

	std::cout << "调整拷贝后的 vector 大小" << std::endl;
	vec_copy.resize(3);
	vec_copy.shrink_to_fit();

	std::cout << "释放部分容量并让对象析构" << std::endl;
	vec_moved.clear();
	vec_moved.shrink_to_fit();
}