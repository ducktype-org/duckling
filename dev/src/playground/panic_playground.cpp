#include <base/except/exceptions.hpp>

struct T {
	~T() { CORE_ASSERT_NOEXCEPT(false, "panic in panic!"); }
};

int main() {
	try {
		T t;
		CORE_PANIC("Fresh, crispy panic for my dudes <3");
	} catch (const base::Panic& p) { p.printToCerr(); }
}
