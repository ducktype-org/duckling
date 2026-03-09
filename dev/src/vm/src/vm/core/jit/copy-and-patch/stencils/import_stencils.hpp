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
		std::array<char, BinarySize> binary;  // why c++, why char
		LLVM_nm_data                 functions[NumFunctions];

		std::span<const std::byte> stencil_binary(size_t index) const {
			return stencil_binary(functions[index]);
		}

		std::span<const std::byte> stencil_binary(const LLVM_nm_data& func_data) const {
			auto begin = reinterpret_cast<const std::byte*>(binary.data() + func_data.place);
			return std::span(begin, begin + func_data.size);
		}
	};

}
