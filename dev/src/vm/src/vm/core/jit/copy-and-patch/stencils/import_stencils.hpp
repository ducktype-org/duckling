#include <array>
#include <cstddef>
#include <string_view>

namespace vm::jit {

	struct LLVM_nm_data {
		const char* name;
		const char* type;
		int         place;
		int         size;
	};

	template<size_t BinarySize, size_t NumFunctions>
	struct Stencils {
		std::array<char, BinarySize> binary;
		LLVM_nm_data                 functions[NumFunctions];

		std::string_view function_binary(size_t index) {
			char* begin = binary.data() + functions[index].place;
			return std::string_view(begin, begin + functions[index].size);
		}
	};

}
