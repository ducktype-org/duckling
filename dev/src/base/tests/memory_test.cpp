#include <base/memory/single_type_memory_pool_allocator.hpp>

#include <tester/tester.hpp>

struct Base {
    int x;
    Base(int a): x(a) {}
    virtual ~Base() = default;
};
struct Derived: public Base {
    int y;

    Derived(int a): Base{a}, y(a * 2) {}
    bool operator==(int a) const {
        return x == a and y == a * 2;
    }

    virtual ~Derived() = default;
};

class MemoryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MemoryTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
        TESTER_ADD_TEST(basicCompilationTest<u64 COMMA 42>);
        TESTER_ADD_TEST(basicCompilationTest<bool COMMA true>);
        TESTER_ADD_TEST(basicCompilationTest<unsigned char COMMA 'A'>);
        TESTER_ADD_TEST(basicCompilationTest<Derived COMMA 123>);
    }

    template<class T, auto initial_value>
	void basicCompilationTest() {
        base::SingleTypeMemoryPoolAllocator<T> t_allocator;

        auto ref = t_allocator.allocateEmplace(initial_value);
        ASSERT_EQUAL(*ref, initial_value);

        t_allocator.deallocateDestroy(ref);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
