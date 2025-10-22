#include <base/memory/single_type_memory_pool_allocator.hpp>

#include <tester/tester.hpp>

class MemoryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MemoryTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
        base::SingleTypeMemoryPoolAllocator<u64> u64_allocator;

        auto ref = u64_allocator.allocateEmplace(42);
       
        ASSERT_EQUAL(*ref, 42);

        u64_allocator.deallocateDestroy(ref);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
