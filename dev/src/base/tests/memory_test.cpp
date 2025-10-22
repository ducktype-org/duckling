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

struct DestructionTracker final {
    inline static u64 destroyed_count = 0;

    DestructionTracker() = default;
    DestructionTracker(const DestructionTracker&) = delete;
    DestructionTracker(DestructionTracker&&) = delete;
    DestructionTracker& operator=(const DestructionTracker&) = delete;
    DestructionTracker& operator=(DestructionTracker&&) = delete;

    ~DestructionTracker() {
        destroyed_count++;
    }
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

    void destructorIsCalledTest() {
        auto allocator = base::SingleTypeMemoryPoolAllocator<DestructionTracker>{};
        
        auto ptr_1 = allocator.allocateEmplace();
        auto ptr_2 = allocator.allocateEmplace();
        auto ptr_3 = allocator.allocateEmplace();


        {
            auto ptr_4 = allocator.allocateEmplace();
            auto ptr_5 = allocator.allocateEmplace();

            ASSERT_EQUAL(DestructionTracker::destroyed_count, 0);

            allocator.deallocateDestroy(ptr_4);
            allocator.deallocateDestroy(ptr_5);

            ASSERT_EQUAL(DestructionTracker::destroyed_count, 2);
        }

        allocator.deallocateDestroy(ptr_1);
        allocator.deallocateDestroy(ptr_2);
        allocator.deallocateDestroy(ptr_3);

        ASSERT_EQUAL(DestructionTracker::destroyed_count, 4);

    }
};

TESTER_COMMON_MAIN("/src/base/tests/");
