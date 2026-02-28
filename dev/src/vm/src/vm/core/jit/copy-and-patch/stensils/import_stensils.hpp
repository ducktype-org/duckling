#include <cstddef>
#include <array>
#include <string_view>

namespace vm::jit {

template<size_t BinarySize, size_t NumFunctions>
struct Stensils {
	struct LLVM_nm_data {
		const char* name;
		const char* type;
		int         place;
		int         size;
	};

	std::array<char, BinarySize>           binary;
	std::array<LLVM_nm_data, NumFunctions> functions;

	std::string_view function_binary(size_t index) {
		char* begin = binary.data() + functions[index].place;
		return std::string_view(begin, begin + functions[index].size);
	}
};

}
