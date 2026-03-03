#include "../memory/memory.cpp"  // for easy invocation
#include "../memory/memory.hpp"
#include "../stencils/import_stencils.hpp"

#include <cstring>
#include <string>

constexpr char binary[] = {
#embed "data-text" suffix(, )
};

vm::jit::Stencils     stencils = vm::jit::Stencils{
		.binary = std::to_array(binary), .functions = {
#include "data-nm"
	}
};

int main() {
	for (char c: stencils.binary) std::cerr << std::hex << (int) (unsigned char) c << ' ';
	std::cerr << std::endl;

	assert(std::string{ stencils.functions[0].name } == std::string{ "foo(int)" });
	assert(std::string{ stencils.functions[0].type } == std::string{ "t" });

	auto memory = JitMemory::allocate(stencils.functions[0].size);
	std::memcpy(memory.memory, stencils.binary.data() + stencils.functions[0].place, stencils.functions[0].size);
	memory.mark_executable();
	auto fibo = memory.into_func<int(int)>();
	assert(std::invoke(fibo, 0) == 1);
	assert(std::invoke(fibo, 1) == 1);
	assert(std::invoke(fibo, 2) == 2);
	assert(std::invoke(fibo, 3) == 3);
	assert(std::invoke(fibo, 4) == 5);
	assert(std::invoke(fibo, 5) == 8);
}
