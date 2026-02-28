#include "../../memory/memory.cpp"  // for easy invocation
#include "../../memory/memory.hpp"
#include "../../stencils/import_stencils.hpp"

#include <cstring>
#include <string>

LLVM_nm_data stencils_offsets[]{};
Stencils     stencils {
		.binary = {
#embed "data-text" suffix(, )
	}, .functions = {
#include "data.nm"
	}
};

int main() {
	for (char c: object_file) std::cerr << std::hex << (int) (unsigned char) c << ' ';
	std::cerr << std::endl;

	assert(std::string{ llvm_nm_data[0].name } == std::string{ "foo(int)" });
	assert(std::string{ llvm_nm_data[0].type } == std::string{ "t" });

	auto memory = JitMemory::allocate(llvm_nm_data[0].size);
	std::memcpy(memory.memory, object_file + llvm_nm_data[0].place, llvm_nm_data[0].size);
	memory.mark_executable();
	auto fibo = memory.into_func<int(int)>();
	assert(std::invoke(fibo, 0) == 1);
	assert(std::invoke(fibo, 1) == 1);
	assert(std::invoke(fibo, 2) == 2);
	assert(std::invoke(fibo, 3) == 3);
	assert(std::invoke(fibo, 4) == 5);
	assert(std::invoke(fibo, 5) == 8);
}
