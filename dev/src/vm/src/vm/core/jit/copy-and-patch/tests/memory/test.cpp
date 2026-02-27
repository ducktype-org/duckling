#include "../../memory/memory.cpp"  // for easy invocation
#include "../../memory/memory.hpp"

#include <string>
#include <cstring>

constexpr char object_file[] = {
#embed "data-text" \
    suffix(,)
};

struct LLVM_nm_data {
	const char* name;
	const char* type;
	int   place;
	int   size;
};

LLVM_nm_data llvm_nm_data[] {
#include "data.nm"
};

int main() {
    for (char c : object_file) {
        std::cerr << std::hex << (int)(unsigned char)c << ' ';
    }
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
