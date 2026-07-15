# C++ 的 new 与 delete

## 1. new 表达式 vs operator new 函数

`new` 表达式是一个**关键字**，不是函数调用。编译器会把它翻译成两步：

```cpp
auto *p = new T(args);
```

等价于：

```cpp
void *tmp = ::operator new(sizeof(T));   // 第一步：分配原始内存
T *p = new (tmp) T(args);               // 第二步：在那块内存上调用构造函数（placement new）
```

`operator new` 只是一个普通的全局函数，**只负责分配原始内存，不调用构造函数**。

同理 `delete p` 翻译为：

```cpp
p->~T();              // 第一步：调用析构函数
::operator delete(p); // 第二步：释放内存
```

**所有 new/delete 表达式的翻译规则：**

| 你写的                | 编译器翻译成                                             |
| --------------------- | -------------------------------------------------------- |
| `new T(args)`       | `operator new(sizeof(T))` + `T::T(args)`             |
| `new T[n]`          | `operator new[](sizeof(T) * n)` + 循环 `T::T()` × n |
| `new (ptr) T(args)` | 不分配，直接 `ptr->T(args)`                            |
| `delete p`          | `p->~T()` + `operator delete(p)`                     |
| `delete[] p`        | 循环 `p[i].~T()` × n + `operator delete[](p)`       |

---

## 2. operator new 的默认实现

标准库提供的默认实现，本质就是 `malloc`/`free` 的包装：

```cpp
void* operator new(std::size_t size) {
    void *p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t size) {
    void *p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void *p) noexcept {
    std::free(p);
}

void operator delete[](void *p) noexcept {
    std::free(p);
}
```

`operator new` 和 `operator new[]` 底层做的事情完全一样，都是 `malloc`。区分存在是设计对称性 + 保留扩展点，实际中几乎没有区别。

---

## 3. 标准库中的全部重载版本

`<new>` 头文件中声明了 16 个全局 operator new/delete 重载：

### 普通版本

```cpp
void* operator new(std::size_t);        // new T
void* operator new[](std::size_t);      // new T[n]
void  operator delete(void*);           // delete p
void  operator delete[](void*);         // delete[] p
```

分配失败时抛 `std::bad_alloc`。

### 带大小的释放（C++14）

```cpp
void operator delete(void*, std::size_t);
void operator delete[](void*, std::size_t);
```

释放时多传一个 `size`，让分配器可以更快回收（不用自己查大小）。编译器自动生成带 size 的调用。

### nothrow 版本

```cpp
void* operator new(std::size_t, const std::nothrow_t&);
void* operator new[](std::size_t, const std::nothrow_t&);
```

分配失败时返回 `nullptr` 而不抛异常：

```cpp
int *p = new (std::nothrow) int;  // 失败返回 nullptr
```

### 对齐版本（C++17）

```cpp
void* operator new(std::size_t, std::align_val_t);
void* operator new[](std::size_t, std::align_val_t);
void  operator delete(void*, std::align_val_t);
void  operator delete[](void*, std::align_val_t);
```

当类型要求超过默认对齐时自动使用：

```cpp
struct alignas(64) CacheLine { char data[64]; };
CacheLine *p = new CacheLine;  // 编译器自动调用对齐版本
```

**分配和释放必须配对**——用了对齐 new，就必须用对齐 delete。

---

## 4. operator new 与 operator new[] 的关系

这两个函数在默认实现层面**完全一样**，都是 `malloc`。区别只存在于类级别重载——你可以为一个类分别重载它们，让单个对象和数组走不同分配策略：

```cpp
class Foo {
public:
    static void* operator new(size_t size) { /* 单对象策略 */ }
    static void* operator new[](size_t size) { /* 数组策略 */ }
};
```

但 `operator new` 和 `operator new[]` 是**两个独立的函数**，查找时互不交叉：

| 表达式       | 查找顺序                                      |
| ------------ | --------------------------------------------- |
| `new T`    | `T::operator new` → `::operator new`     |
| `new T[n]` | `T::operator new[]` → `::operator new[]` |

如果类只重载了 `operator new`，`new T[n]` 不会使用它，而是直接用全局的 `::operator new[]`。

---

## 5. 默认初始化 vs 值初始化

`new` 表达式中的括号 `()` 决定初始化方式：

| 表达式         | 初始化方式         | int 的结果         | 有构造函数的类 |
| -------------- | ------------------ | ------------------ | -------------- |
| `new T`      | 默认初始化         | 未初始化（垃圾值） | 调用默认构造   |
| `new T()`    | 值初始化           | 零初始化（= 0）    | 调用默认构造   |
| `new T[n]`   | 默认初始化每个元素 | 未初始化           | 调用默认构造   |
| `new T[n]()` | 值初始化每个元素   | 零初始化           | 调用默认构造   |

对有构造函数的类类型，`new T` 和 `new T()` 都会调用默认构造函数，效果一样。区别只在基本类型上：是否清零。

---

## 6. Placement new

`new (ptr) T(args)` 调用的 placement `operator new` 是一个透传函数：

```cpp
void* operator new(std::size_t, void* __p) noexcept { return __p; }
```

它什么都不做，原样返回传入的指针。所以 placement new 的实际效果就是**在已有内存上调用构造函数，不分配新内存**。

```cpp
void *buf = ::operator new[](1024);       // 分配一大块原始内存
new (buf) T(args);                         // 在 buf 上构造对象，不分配新内存
```

### 注意：不会自动析构旧对象

在已有对象上 placement new 是**未定义行为**（除非先调用析构函数）：

```cpp
std::vector<int> vec;                       // 已构造
new(&vec) std::vector<int>(3);             // 未定义行为！旧 vector 的内部内存泄漏
```

正确做法：

```cpp
std::vector<int> vec;                       // 已构造
vec.~vector();                              // 先析构
new(&vec) std::vector<int>(3);             // 再构造
```

---

## 7. ::作用域的意义

`::operator new` 表示从**全局命名空间**查找，跳过类内重载：

```cpp
::operator new(size)   // 全局版本，系统的
operator new(size)     // 先找当前作用域/类的重载，找不到才用全局的
```

内存池中使用 `::operator new[]` 是为了避免被类重载的 `operator new` 干扰。
