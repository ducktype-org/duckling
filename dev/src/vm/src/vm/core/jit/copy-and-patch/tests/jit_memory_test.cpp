#include "../memory/memory.cpp"
#include "../memory/memory.hpp"
#include "../stencils/dynamic_linker.cpp"
#include "../stencils/import_stencils.hpp"

#include <base/pointers/box.hpp>
#include <base/preproc/diagnostics.hpp>

#include <tester/tester.hpp>

#include <cstring>
#include <string>

using vm::jit::cnp::JitMemory;
using vm::jit::cnp::LLVM_nm_data;
using vm::jit::cnp::Stencils;


PUSH_DIAGNOSTIC ALLOW_EXTENSIONS constexpr static char full_elf[] = {
#embed "mock_stencils-so" suffix(, )
};
POP_DIAGNOSTIC

static auto stencils = Stencils { .binary = std::bit_cast<std::array<byte, sizeof(full_elf)>>(full_elf),
	                                                             .functions = {
#include "mock_stencils-nm"
																 }
}
.load();

class JitMemoryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS JitMemoryTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSimple);
		TESTER_ADD_TEST(testRecursive);
		TESTER_ADD_TEST(testCallingSimple);
		TESTER_ADD_TEST(testCallingRecursive);
		TESTER_ADD_TEST(testCallingLibc);
	}

private:
	void printBinary() {
		std::cerr << "Binary:\n";
		for (byte c: stencils.binary())
			std::cerr << std::hex << (int) (unsigned char) c << ' ';
		std::cerr << "\nFunctions:\n";
		for (LLVM_nm_data data: stencils.functions()) std::cerr << data.name << '\n';
	}

	constexpr static auto find_func = [](auto name) {
		for (LLVM_nm_data data: stencils.functions())
			if (data.name == name) return data;
		CORE_PANIC("No function with that name");
	};

	void testSimple() {
		auto foo_code = find_func("simple_function_plus_1");
		auto memory   = JitMemory::allocate(foo_code.size);
		std::ranges::copy(stencils.stencil_binary(foo_code), memory.memory);
		memory.mark_executable();

		auto simple = memory.into_func<int(int)>();
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), i + 1);
	}

	void testRecursive() {
		auto foo_code = find_func("recursive_fibonacci");
		auto memory   = JitMemory::allocate(foo_code.size);
		std::ranges::copy(stencils.stencil_binary(foo_code), memory.memory);
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
		auto simple = stencils.dynlib.findSymbol<int(int)>("calling_simple_odd");
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), 2 * i + 1);
	}

	void testCallingRecursive() {
		auto fibonacci_sum = stencils.dynlib.findSymbol<int(int)>("calling_fibonacci_sum");

		ASSERT_EQUAL(std::invoke(fibonacci_sum, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 1), 2);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 2), 6);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 3), 15);
	}

	void testCallingLibc() {
		auto calling_libc    = stencils.dynlib.findSymbol<int*(int)>("calling_libc");
		auto from_jit_memory = base::Box<int>::fromPointer(std::invoke(calling_libc, 100));
		for (int i = 0; i < 100; ++i) ASSERT_EQUAL(from_jit_memory.get()[i], i);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/jit/");
