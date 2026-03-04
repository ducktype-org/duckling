#include "../memory/memory.cpp"  // for easy invocation
#include "../memory/memory.hpp"
#include "../stencils/import_stencils.hpp"

#include <base/preproc/diagnostics.hpp>

#include <tester/tester.hpp>

#include <cstring>
#include <string>

class JitMemoryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS JitMemoryTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testJitMemory); }

private:
	void testJitMemory() {
		PUSH_DIAGNOSTIC ALLOW_EXTENSIONS;
		constexpr char  binary[] = {
#embed "mock_stencils-text" suffix(, )
		};
		POP_DIAGNOSTIC

		constexpr vm::jit::Stencils stencils = vm::jit::Stencils{ .binary = std::to_array(binary),
			                                                      .functions = {
#include "mock_stencils-nm"
																  } };

		std::cerr << "Binary:\n";
		for (char c: stencils.binary) std::cerr << std::hex << (int) (unsigned char) c << ' ';
		std::cerr << std::endl;

		std::cerr << "Functions:\n";
		auto find_func = [&](auto name) {
			for (vm::jit::LLVM_nm_data data: stencils.functions) {
				std::cerr << data.name << '\n';
				if (data.name == name) return data;
			}
			CORE_PANIC("No function with that name");
		};

		auto foo_code = find_func("foo");

		auto memory = JitMemory::allocate(foo_code.size);
		std::memcpy(memory.memory, stencils.binary.data() + foo_code.place, foo_code.size);
		memory.mark_executable();
		auto fibo = memory.into_func<int(int)>();
		assert(std::invoke(fibo, 0) == 1);
		assert(std::invoke(fibo, 1) == 1);
		assert(std::invoke(fibo, 2) == 2);
		assert(std::invoke(fibo, 3) == 3);
		assert(std::invoke(fibo, 4) == 5);
		assert(std::invoke(fibo, 5) == 8);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/jit//");
