// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/memory/single_type_memory_pool_allocator.hpp>

#include <tester/tester.hpp>

struct Base {
	int x;

	Base(int a): x(a) {}

	virtual ~Base() = default;
};

struct Derived: public Base {
	int y;

	Derived(int a): Base{ a }, y(a * 2) {}

	bool operator==(int a) const { return x == a and y == a * 2; }

	~Derived() override = default;
};

struct DestructionTracker final {
	inline static u64 destroyed_count = 0;

	DestructionTracker()                                     = default;
	DestructionTracker(const DestructionTracker&)            = delete;
	DestructionTracker(DestructionTracker&&)                 = delete;
	DestructionTracker& operator=(const DestructionTracker&) = delete;
	DestructionTracker& operator=(DestructionTracker&&)      = delete;

	~DestructionTracker() { destroyed_count++; }
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
		TESTER_ADD_TEST(destructorIsCalledTest);
		TESTER_ADD_TEST(largeTest);
		TESTER_ADD_TEST(largerTest);
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

		ASSERT_EQUAL(DestructionTracker::destroyed_count, 5);
	}

	void largeTest() {
		constexpr u64 ALLOCATION_COUNT = 100'000;

		auto allocator = base::SingleTypeMemoryPoolAllocator<u64>{};

		std::vector<base::Ref<u64>> allocated_ptrs;
		allocated_ptrs.reserve(ALLOCATION_COUNT);

		for (u64 i = 0; i < ALLOCATION_COUNT; i++) {
			auto ptr = allocator.allocateEmplace(i);
			allocated_ptrs.push_back(ptr);
		}

		for (u64 i = 0; i < ALLOCATION_COUNT; i++) ASSERT_TRUE(*allocated_ptrs[i] == i);

		for (u64 i = 0; i < ALLOCATION_COUNT; i++) allocator.deallocateDestroy(allocated_ptrs[i]);
	}

	void largerTest() {
		constexpr u64 ALLOCATION_COUNT = 1'000'000;

		auto allocator = base::SingleTypeMemoryPoolAllocator<u64>{};

		std::vector<base::Ref<u64>> allocated_ptrs;
		allocated_ptrs.reserve(ALLOCATION_COUNT);

		for (u64 i = 0; i < ALLOCATION_COUNT; i++) {
			auto ptr = allocator.allocateEmplace(i);
			allocated_ptrs.push_back(ptr);
		}

		for (u64 i = 0; i < ALLOCATION_COUNT; i++) ASSERT_TRUE(*allocated_ptrs[i] == i);

		// notice that here we use justDestroy, instead of deallocateDestroy:
		for (u64 i = 0; i < ALLOCATION_COUNT; i++) allocator.justDestroy(allocated_ptrs[i]);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
