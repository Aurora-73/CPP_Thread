#include <iostream>
#include <memory>
#include <vector>

/* 内存池原理： 申请一大块内容，然后将其切分为若干小块，申请只申请一次
*/

class FixedSizePool {
public:
	explicit FixedSizePool(size_t block_size, size_t blocks_per_page) :
	    blocks_per_page_(blocks_per_page), free_list_head(nullptr) {
		constexpr size_t ptr_size = sizeof(void *);
		block_size_ = (block_size + ptr_size - 1) / ptr_size * ptr_size;
	}

	~FixedSizePool() {
		for(auto &page : pages_) {
			::operator delete[](page);
		}
	}

	void *allocate() {
		if(!free_list_head) expand();
		Node *curr = free_list_head;
		free_list_head = free_list_head->next;
		return curr;
	}

	void deallocate(void *p) {
		if(!p) return;
		Node *curr = static_cast<Node *>(p);
		curr->next = free_list_head;
		free_list_head = curr;
	}

	size_t block_size() const {
		return block_size_;
	};

	size_t blocks_per_page() const {
		return blocks_per_page_;
	};

private:
	void expand() {
		pages_.emplace_back(::operator new[](blocks_per_page_ * block_size_));
		char *base_ptr = static_cast<char *>(pages_.back());
		// void * 没有大小概念，标准 C++ 不允许对 void * 做指针算术。GCC 作为扩展允许它（按 1 字节步长），但 MSVC 会直接报错，且这是未定义行为。应先转为 char *：
		// 这里之所以转为 void * 是因为 char 的大小就是一字节，而指针的加法是一次偏移一个指向类型的大小，因为 sizeof(char) == 1，因此使用 char* 进行取地址
		for(size_t i = 0; i < blocks_per_page_; ++i) {
			Node *curr = reinterpret_cast<Node *>(base_ptr + i * block_size_);
			// reinterpret_cast 是将二进制内存按照指定的类型重新解释，即直接将对应的内存区域当成所需的类型
			curr->next = free_list_head;
			free_list_head = curr;
		}
	}

	struct Node {
		Node *next;
	};
	size_t block_size_;
	size_t blocks_per_page_;
	Node *free_list_head;
	std::vector<void *> pages_;
};

struct point {
	float x, y, z;

	static void *operator new(size_t n);
	static void operator delete(void *p) noexcept;
};

inline static FixedSizePool mp {sizeof(point), 5000};

void *point::operator new(size_t n) {
	if(n > mp.block_size()) throw std::bad_alloc();
	return mp.allocate();
};

void point::operator delete(void *p) noexcept {
	mp.deallocate(p);
};

int main() {
	std::vector<point *> vec;

	std::cout << vec.size() << std::endl;
	vec.reserve(10000);

	for(int i = 0; i < 10000; ++i) {
		vec.emplace_back(new point);
	}
	std::cout << vec.size() << std::endl;

	// ..........................

	for(auto *p : vec) {
		delete p;
	}
}
