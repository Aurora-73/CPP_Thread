// 分配器 allocator
#include <iostream>
#include <memory>

/*
    分配器用于实现容器算法的存储细节隔离，从而解耦合。
    分配器提供存储分配与释放的标准方法
    实现对象分配和构造分离

【std::allocator】 定义于头文件 <memory>
    template<class T> struct allocator;
    std::allocator 类模板是所有标准库容器在未提供用户指定分配器时使用的默认 Allocator。
    默认分配器是无状态的，即给定分配器的所有实例都是可互换的，比较相等，并且可以<释放由同一分配器类型的任何其他实例分配的内存>。

成员变量
    value_type	T
    propagate_on_container_move_assignment	std::true_type 用于指示在容器进行拷贝赋值操作时，是否需要将分配器（allocator）也进行相应的拷贝。
    is_always_equal 表示：任意两个该类型的 allocator 对象是否总是等价。对于无状态 allocator，通常为 true；对于保存状态的 allocator，通常为 false。
        std::allocator 是无状态 allocator，因此 is_always_equal=true。它本质上只是对 ::operator new 和 ::operator delete 的简单封装。
    template< class U > struct rebind
        {
            typedef allocator<U> other;
        }; rebind (C++17 中已弃用)
        allocator 的类型重绑定机制（allocator rebinding），用于解决 一个 allocator 通常只为某一种 T 分配内存，但 STL 容器内部经常需要分配其他类型的对象的问题。
        例如：std::list<int, MyAllocator<int>> 内部存储的不是 int 而是 struct Node { Node* next; int value; }; 实际需要 MyAllocator<Node> 这时候就需要 rebind。

成员函数
    address(const T& x)  返回 x 的实际地址，即使存在重载的 operator&。

    T* allocate(std::size_t n); (1)
    T* allocate( std::size_t n, const void* hint ); (2)
        分配 n * sizeof(T) 字节的未初始化存储空间，通过调用 ::operator new(std::size_t)
        指针 hint 可用于提供引用局部性：分配器会试图分配 尽可能接近 hint 的新内存块。
        allocate 只会分配内存，返回指向 T 类型 n 个对象数组的第一个元素的指针，但是不会构造这些对象。

    void deallocate( T* p, std::size_t n ); 
        解分配指针 p 所引用的存储，它必须是通过先前对 allocate() 的调用所获得的指针。参数 n 必须等于产生 p 的 allocate第一个参数。
        调用 ::operator delete(void*)，但未指定何时以及如何调用它。

    size_type max_size() const noexcept; 
        返回支持的最大分配大小。在大多数实现中，这返回 std::numeric_limits<size_type>::max() / sizeof(value_type)。

    template< class U, class... Args > void construct( U* p, Args&&... args ); (C++17 中已弃用)
        在由 p 指向的已分配未初始化存储中，使用全局就地构造（placement-new）构造类型 T 的对象。调用 ::new((void*)p) U(std::forward<Args>(args)...)。


    void destroy( pointer p ); (1)	(C++11 前)
    template< class U > void destroy( U* p ); (2)	(C++17 中已弃用)
        调用 p 指向的对象的析构函数。
        1) 调用 p->~T()。
        2) 调用 p->~U()。

    operator==,!= 
        比较两个默认分配器。因为默认分配器是无状态的，所以两个默认分配器始终相等。


【allocator_traits】 定义于头文件 <memory>
    template< class Alloc > struct allocator_traits;
    allocator_traits 类模板提供了一种标准化的方式来访问 Allocator 的各种属性。
    标准容器和其他标准库组件通过此模板访问分配器，从而可以使用任何类类型作为分配器，只要用户提供的 std::allocator_traits 特化实现了所有必需的功能。

成员变量
    allocator_type	                        Alloc
    value_type	                            Alloc::value_type
    propagate_on_container_copy_assignment	容器拷贝时是否需要同时拷贝分配器，若分配器未定义该值则默认为 std::false_type
    propagate_on_container_move_assignment	容器移动时是否需要同时拷贝分配器，若分配器未定义该值则默认为 std::false_type
    propagate_on_container_swap	            容器交换时是否需要同时拷贝分配器，若分配器未定义该值则默认为 std::false_type
    is_always_equal	                        给定分配器的所有实例都是可互换的，若分配器未定义该值则默认为 std::is_empty<Alloc>::type
    rebind_alloc<T>	                        Alloc::rebind<T>::other，若不存在且此 Alloc 形式为 SomeAllocator<U, Args>，则为 SomeAllocator<T, Args>
    rebind_traits<T>	                    std::allocator_traits<rebind_alloc<T>>

成员函数
    static pointer allocate( Alloc& a, size_type n ); 
    static pointer allocate( Alloc& a, size_type n, const_void_pointer hint );
        使用分配器 a 来分配 n * sizeof(Alloc::value_type) 字节的未初始化存储。
        指针 hint 可用于提供引用局部性：分配器会试图分配 尽可能接近 hint 的新内存块。
        只会分配内存，返回指向 T 类型 n 个对象数组的第一个元素的指针，但是不会构造这些对象。
    
    static void deallocate( Alloc& a, pointer p, size_type n );
        使用分配器 a 通过调用 a.deallocate(p, n) 来释放 p 引用的存储。

    template< class T, class... Args > static void construct( Alloc& a, T* p, Args&&... args );
        在 p 位置构造对象。
            若 Alloc 有成员函数 construct()，则通过调用 a.construct(p, std::forward<Args>(args)...)，在 p 所指向的已分配的未初始化存储中构造 T 类型的对象。
            否则调用 ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...) (placement new)
    
    template< class T > static void destroy( Alloc& a, T* p );
        调用 p 所指向对象的析构函数。
            若 Alloc 有成员函数 destroy()，通过调用 a.destroy(p) 来实现。
            否则直接调用 *p 的析构函数，即 p->~T() (析构函数)。
    
    static size_type max_size( const Alloc& a ) noexcept;
        若 Alloc 有成员函数 max_size()，通过调用 a.max_size()，从分配器 a 获取理论上最大可能的分配大小。
        否则返回 std::numeric_limits<size_type>::max() / sizeof(value_type)。
*/

using namespace std;

class TestClass {
	int a;

public:
	TestClass() {
		std::cout << "TestClass constructor called." << std::endl;
	}
	TestClass(int val) : a(val) {
		std::cout << "TestClass constructor with param " << val << " called." << std::endl;
	}
	~TestClass() noexcept {
		std::cout << "TestClass destructor called." << std::endl;
	}
};

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

int main() {
	std::cout << "==========  std::allocator 的基本功能 ==========" << std::endl;

	// 1. allocator 对象的创建和类型特性
	allocator<TestClass> alloc;
	allocator<TestClass> alloc2;

	std::cout << "\n【1. allocator 的基本属性】" << std::endl;
	// 检查两个分配器是否相等（无状态分配器总是相等）
	std::cout << "alloc == alloc2: " << (alloc == alloc2 ? "true" : "false") << std::endl;
	std::cout << "alloc != alloc2: " << (alloc != alloc2 ? "true" : "false") << std::endl;

	// 2. 获取最大分配大小
	std::cout << "\n【2. 最大分配大小】" << std::endl;
	using alloc_traits = allocator_traits<allocator<TestClass>>;
	std::cout << "max_size: " << alloc_traits::max_size(alloc) << std::endl;

	// 3. allocate 分配内存（不构造对象）
	std::cout << "\n【3. 分配内存（不构造对象）】" << std::endl;
	int cnt = 5;
	auto memo = alloc.allocate(cnt); // 分配 5 * sizeof(TestClass) 的空间，但不调用构造函数
	std::cout << "已分配 " << cnt << " 个 TestClass 对象的空间" << std::endl;

	// 4. 使用 allocator_traits 进行构造和析构
	std::cout << "\n【4. 使用 allocator_traits 进行对象构造】" << std::endl;
	// alloc_traits 已在前面定义

	//  allocator_traits 的属性
	std::cout << "allocator_traits::value_type: " << typeid(alloc_traits::value_type).name() << std::endl;
	std::cout << "is_always_equal: " << (alloc_traits::is_always_equal::value ? "true" : "false") << std::endl;
	std::cout << "propagate_on_container_copy_assignment: "
	          << (alloc_traits::propagate_on_container_copy_assignment::value ? "true" : "false") << std::endl;
	std::cout << "propagate_on_container_move_assignment: "
	          << (alloc_traits::propagate_on_container_move_assignment::value ? "true" : "false") << std::endl;

	// 使用 allocator_traits 调用构造函数
	std::cout << "\n【5. 构造前 5 个对象】" << std::endl;
	for(int i = 0; i < cnt; ++i) {
		alloc_traits::construct(alloc, &memo[i], i * 10); // 使用参数进行构造
	}

	// 6.  address() 函数
	std::cout << "\n【6. 获取对象地址】" << std::endl;
	// 注：address() 在 C++20 中已弃用，直接使用取地址运算符
	TestClass *addr = &memo[0];
	std::cout << "memo[0] 的地址: " << (void *)addr << std::endl;

	// 7.  rebind 机制
	std::cout << "\n【7. allocator rebind（类型重绑定）】" << std::endl;
	using int_alloc = allocator_traits<allocator<TestClass>>::rebind_alloc<int>;
	int_alloc int_allocator;
	auto int_mem = int_allocator.allocate(10);
	std::cout << "已使用 rebind 分配了 10 个 int 对象的空间" << std::endl;
	int_allocator.deallocate(int_mem, 10);

	// 8.  allocator_traits 的静态成员函数
	std::cout << "\n【8. 使用 allocator_traits 的静态成员函数】" << std::endl;
	using double_alloc_traits = allocator_traits<allocator<double>>;
	allocator<double> double_alloc;
	auto double_mem = double_alloc_traits::allocate(double_alloc, 3); // 为 double 分配内存
	std::cout << "通过 allocator_traits::allocate 分配了内存" << std::endl;
	double_alloc_traits::deallocate(double_alloc, double_mem, 3);

	// 9.  destroy 和 deallocate
	std::cout << "\n【9. 调用析构函数并释放内存】" << std::endl;
	for(int i = 0; i < cnt; ++i) {
		alloc_traits::destroy(alloc, &memo[i]); // 调用析构函数
	}
	alloc.deallocate(memo, cnt); // 释放内存
	std::cout << "已释放内存" << std::endl;

	// 10.  allocate 的 hint 参数（现代 C++ 已弃用）
	std::cout << "\n【10. allocate 】" << std::endl;
	// 注：allocate(n, hint) 在现代 C++ 中已弃用，仅保留 allocate(n)
	auto memo1 = alloc.allocate(3);
	auto memo2 = alloc.allocate(3); // 普通分配
	std::cout << "memo1 地址: " << (void *)memo1 << std::endl;
	std::cout << "memo2 地址: " << (void *)memo2 << std::endl;
	alloc.deallocate(memo1, 3);
	alloc.deallocate(memo2, 3);

	return 0;
}