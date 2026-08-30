#include "../memory/memory.hpp"
#include "../stencils/import_stencils.hpp"

#include <base/pointers/box.hpp>
#include <base/preproc/diagnostics.hpp>

#include <tester/tester.hpp>

#include <cstring>
#include <string>

using vm::jit::cnp::HoleValue;
using vm::jit::cnp::JitFuncMemory;
using vm::jit::cnp::StencilData;
using vm::jit::cnp::StencilHole;
using vm::jit::cnp::Stencils;

// NOLINTBEGIN
PUSH_DIAGNOSTIC
ALLOW_EXTENSIONS
static constexpr char binary[] = {
#if __has_embed("mock_stencils-so")
	#embed "mock_stencils-so"
#endif
};
POP_DIAGNOSTIC
// NOLINTEND

static auto stencils = Stencils{
// Linter doesn't actually build mock_stencils-nm so it would be unavailable.
#if __has_include(<mock_stencils-nm>)
			.stencils_binary = std::bit_cast<std::array<std::byte, sizeof(binary)>>(binary),
			.stencils_data =
	#include <mock_stencils-nm>
#endif
		}.load().value();

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
		TESTER_ADD_TEST(testPatching);
		TESTER_ADD_TEST(testCombining);
	}

private:
	constexpr static auto FIND_FUNC = [](auto name) {
		for (StencilData data: stencils.stencilsData())
			if (data.name == name) return data;
		CORE_PANIC("No function with that name");
	};

	void testSimple() {
		auto foo_code      = FIND_FUNC("simple_function_plus_1");
		auto memory_result = JitFuncMemory::allocate(foo_code.size);
		ASSERT_HAS_VALUE(memory_result);
		auto& memory = *memory_result;
		stencils.relocate(foo_code, memory.addr);
		ASSERT_HAS_VALUE(memory.markExecutable());

		auto simple = memory.intoFunc<int(int)>();
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), i + 1);
	}

	void testRecursive() {
		auto foo_code      = FIND_FUNC("recursive_fibonacci");
		auto memory_result = JitFuncMemory::allocate(foo_code.size);
		ASSERT_HAS_VALUE(memory_result);
		auto& memory = *memory_result;
		stencils.relocate(foo_code, memory.addr);
		ASSERT_HAS_VALUE(memory.markExecutable());

		auto fibonacci = memory.intoFunc<int(int)>();
		ASSERT_EQUAL(std::invoke(fibonacci, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci, 1), 1);
		ASSERT_EQUAL(std::invoke(fibonacci, 2), 2);
		ASSERT_EQUAL(std::invoke(fibonacci, 3), 3);
		ASSERT_EQUAL(std::invoke(fibonacci, 4), 5);
		ASSERT_EQUAL(std::invoke(fibonacci, 5), 8);
	}

	void testCallingSimple() {
		auto foo_code      = FIND_FUNC("calling_simple_odd");
		auto memory_result = JitFuncMemory::allocate(foo_code.size);
		ASSERT_HAS_VALUE(memory_result);
		auto& memory = *memory_result;
		stencils.relocate(foo_code, memory.addr);
		ASSERT_HAS_VALUE(memory.markExecutable());

		auto simple = memory.intoFunc<int(int)>();
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), 2 * i + 1);
	}

	void testCallingRecursive() {
		auto foo_code      = FIND_FUNC("calling_fibonacci_sum");
		auto memory_result = JitFuncMemory::allocate(foo_code.size);
		ASSERT_HAS_VALUE(memory_result);
		auto& memory = *memory_result;
		stencils.relocate(foo_code, memory.addr);
		ASSERT_HAS_VALUE(memory.markExecutable());

		auto fibonacci_sum = memory.intoFunc<int(int)>();
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 1), 2);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 2), 6);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 3), 15);
	}

	void testCallingLibc() {
		auto foo_code      = FIND_FUNC("calling_libc");
		auto memory_result = JitFuncMemory::allocate(foo_code.size);
		ASSERT_HAS_VALUE(memory_result);
		auto& memory = *memory_result;
		stencils.relocate(foo_code, memory.addr);
		ASSERT_HAS_VALUE(memory.markExecutable());

		auto calling_libc    = memory.intoFunc<int*(int)>();
		auto from_jit_memory = base::Box<int>::fromPointer(std::invoke(calling_libc, 100));
		for (int i = 0; i < 100; ++i) ASSERT_EQUAL(from_jit_memory.get()[i], i);
	}

	void testPatching() {
		auto foo_code = FIND_FUNC("must_patch");
		auto memory_result = JitFuncMemory::allocate(foo_code.size);
		ASSERT_HAS_VALUE(memory_result);
		auto& memory = *memory_result;
		stencils.relocate(foo_code, memory.addr);
		foo_code.patch(memory.addr, [](HoleValue value) {
			if (value == HoleValue::Arg0)
				return 9;
			else
				CORE_PANIC("Unexpected relocation");
		});
		memory.markExecutable();

		auto must_patch = memory.intoFunc<int(int)>();
		for (int i = 0; i < 100; ++i) ASSERT_EQUAL_PRINT(std::invoke(must_patch, i), i + 9);
	}

	void testCombining() {
		auto add_code = FIND_FUNC("mock_add");
		auto mul_code = FIND_FUNC("mock_mul");
		auto end_code = FIND_FUNC("mock_end");

		auto memory_result = JitFuncMemory::allocate(add_code.size + mul_code.size + end_code.size);
		ASSERT_HAS_VALUE(memory_result);
		auto& memory = *memory_result;

		auto add_addr = memory.addr;
		auto mul_addr = stencils.relocate(add_code, add_addr);
		auto end_addr = stencils.relocate(mul_code, mul_addr);
		stencils.relocate(end_code, end_addr);

		add_code.patch(add_addr, [&](HoleValue hole) {
			switch (hole) {
			case HoleValue::ContinueFn:
				return std::bit_cast<std::intptr_t>(mul_addr);
			default:
				CORE_PANIC("unexpected relocation");
			}
		});

		mul_code.patch(mul_addr, [&](HoleValue hole) {
			switch (hole) {
			case HoleValue::ContinueFn:
				return std::bit_cast<std::intptr_t>(end_addr);
			default:
				CORE_PANIC("unexpected relocation");
			}
		});

		memory.markExecutable();
		auto build_func = memory.intoFunc<int(int, int)>();  // (a + b) * b
		for (int a = 0; a < 10; ++a) {
			for (int b = 0; b < 10; ++b) {
				auto returned = std::invoke(build_func, a, b);
				ASSERT_EQUAL_PRINT(returned, (a + b) * b);
			}
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/jit/");
