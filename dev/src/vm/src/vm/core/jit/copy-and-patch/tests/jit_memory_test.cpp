#include "../memory/memory.hpp"
#include "../stencils/import_stencils.hpp"

#include <base/pointers/box.hpp>
#include <base/preproc/diagnostics.hpp>

#include <tester/tester.hpp>

#include <cstring>
#include <string>

using vm::jit::cnp::JitFuncMemory;
using vm::jit::cnp::StencilData;
using vm::jit::cnp::Stencils;

PUSH_DIAGNOSTIC
ALLOW_EXTENSIONS
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
constexpr static char FULL_ELF[] = {
#if __has_embed("mock_stencils-so")
	#embed "mock_stencils-so"
#else
	0
#endif
};
POP_DIAGNOSTIC

static auto stencils
	= Stencils{ .stencils_binary    = std::bit_cast<std::array<byte, sizeof(FULL_ELF)>>(FULL_ELF),
	            .stencils_data = std::array {
#if __has_include(<mock_stencils-nm>)
	#include <mock_stencils-nm>
#else
			StencilData{}
#endif
					}
				 }.load();

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
	constexpr static auto FIND_FUNC = [](auto name) {
		for (StencilData data: stencils.stencilsData())
			if (data.name == name) return data;
		CORE_PANIC("No function with that name");
	};

	void testSimple() {
		auto foo_code = FIND_FUNC("simple_function_plus_1");
		auto memory   = JitFuncMemory::allocate(foo_code.size);
		stencils.relocate(foo_code, memory.addr);
		memory.markExecutable();

		auto simple = memory.intoFunc<int(int)>();
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), i + 1);
	}

	void testRecursive() {
		auto foo_code = FIND_FUNC("recursive_fibonacci");
		auto memory   = JitFuncMemory::allocate(foo_code.size);
		stencils.relocate(foo_code, memory.addr);
		memory.markExecutable();

		auto fibonacci = memory.intoFunc<int(int)>();
		ASSERT_EQUAL(std::invoke(fibonacci, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci, 1), 1);
		ASSERT_EQUAL(std::invoke(fibonacci, 2), 2);
		ASSERT_EQUAL(std::invoke(fibonacci, 3), 3);
		ASSERT_EQUAL(std::invoke(fibonacci, 4), 5);
		ASSERT_EQUAL(std::invoke(fibonacci, 5), 8);
	}

	void testCallingSimple() {
		auto foo_code = FIND_FUNC("calling_simple_odd");
		auto memory   = JitFuncMemory::allocate(foo_code.size);
		stencils.relocate(foo_code, memory.addr);
		memory.markExecutable();

		auto simple = memory.intoFunc<int(int)>();
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), 2 * i + 1);
	}

	void testCallingRecursive() {
		auto foo_code = FIND_FUNC("calling_fibonacci_sum");
		auto memory   = JitFuncMemory::allocate(foo_code.size);
		stencils.relocate(foo_code, memory.addr);
		memory.markExecutable();

		auto fibonacci_sum = memory.intoFunc<int(int)>();
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 1), 2);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 2), 6);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 3), 15);
	}

	void testCallingLibc() {
		auto foo_code = FIND_FUNC("calling_libc");
		auto memory   = JitFuncMemory::allocate(foo_code.size);
		stencils.relocate(foo_code, memory.addr);
		memory.markExecutable();

		auto calling_libc    = memory.intoFunc<int*(int)>();
		auto from_jit_memory = base::Box<int>::fromPointer(std::invoke(calling_libc, 100));
		for (int i = 0; i < 100; ++i) ASSERT_EQUAL(from_jit_memory.get()[i], i);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/jit/");
