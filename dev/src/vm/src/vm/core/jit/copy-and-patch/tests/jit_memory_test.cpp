#include "../memory/memory.cpp"  // for easy invocation
#include "../memory/memory.hpp"
#include "../stencils/import_stencils.hpp"

#include <base/preproc/diagnostics.hpp>
#include <base/pointers/box.hpp>

#include <tester/tester.hpp>

#include <cstring>
#include <string>

class JitMemoryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS JitMemoryTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSimple);
		TESTER_ADD_TEST(testRecursive);
		TESTER_ADD_TEST(testCallingSimple);
	}

private:
	PUSH_DIAGNOSTIC
	ALLOW_EXTENSIONS
	constexpr static char binary[] = {
#embed "mock_stencils-text" suffix(, )
	};
	POP_DIAGNOSTIC

	constexpr static vm::jit::Stencils stencils
		= vm::jit::Stencils{ .binary    = std::to_array(binary),
		                     .functions = {
#include "mock_stencils-nm"
							 } };

	void printBinary() {
		std::cerr << "Binary:\n";
		for (char c: stencils.binary) std::cerr << std::hex << (int) (unsigned char) c << ' ';
		std::cerr << "\nFunctions:\n";
		for (vm::jit::LLVM_nm_data data: stencils.functions) std::cerr << data.name << '\n';
	}

	constexpr static auto find_func = [](auto name) {
		for (vm::jit::LLVM_nm_data data: stencils.functions)
			if (data.name == name) return data;
		CORE_PANIC("No function with that name");
	};

	void testSimple() {
		auto foo_code = find_func("simple_function_plus_1");

		auto memory = JitMemory::allocate(foo_code.size);
		std::memcpy(memory.memory, stencils.binary.data() + foo_code.place, foo_code.size);
		memory.mark_executable();
		auto simple = memory.into_func<int(int)>();
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), i + 1);
	}

	void testRecursive() {
		auto foo_code = find_func("recursive_fibonacci");

		auto memory = JitMemory::allocate(foo_code.size);
		std::memcpy(memory.memory, stencils.binary.data() + foo_code.place, foo_code.size);
		memory.mark_executable();
		auto fibonacci = memory.into_func<int(int)>();
		ASSERT_EQUAL(std::invoke(fibonacci, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci, 1), 1);
		ASSERT_EQUAL(std::invoke(fibonacci, 2), 2);
		ASSERT_EQUAL(std::invoke(fibonacci, 3), 3);
		ASSERT_EQUAL(std::invoke(fibonacci, 4), 5);
		ASSERT_EQUAL(std::invoke(fibonacci, 5), 8);
	}

	void testCallingSimple() {
		auto foo_code = find_func("calling_simple_odd");

		auto memory = JitMemory::allocate(foo_code.size);
		std::memcpy(memory.memory, stencils.binary.data() + foo_code.place, foo_code.size);
		memory.mark_executable();
		auto simple = memory.into_func<int(int)>();
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), 2 * i + 1);
	}

	void testCallingRecursive() {
		auto foo_code = find_func("calling_fibonacci_sum");

		auto memory = JitMemory::allocate(foo_code.size);
		std::memcpy(memory.memory, stencils.binary.data() + foo_code.place, foo_code.size);
		memory.mark_executable();
		auto fibonacci_sum = memory.into_func<int(int)>();
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 1), 2);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 2), 6);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 3), 15);
	}

	void testCallingLibc() {
		auto foo_code = find_func("calling_fibonacci_sum");

		auto memory = JitMemory::allocate(foo_code.size);
		std::memcpy(memory.memory, stencils.binary.data() + foo_code.place, foo_code.size);
		memory.mark_executable();
		auto calling_libc = memory.into_func<int*(int)>();

		auto from_jit_memory = base::Box<int>::fromPointer(calling_libc(100));
		for (int i = 0; i < 100; ++i) ASSERT_EQUAL(from_jit_memory.get()[i], i);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/jit/");
