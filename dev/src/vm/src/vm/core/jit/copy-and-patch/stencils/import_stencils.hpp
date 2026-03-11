#pragma once

#include "dynamic_linker.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

namespace vm::jit {
	struct LLVM_nm_data {
		const char* name;
		const char* type;
		int         place;
		int         size;
	};

	template<size_t BinarySize, size_t NumFunctions>
	struct LoadedStencils;

	template<size_t BinarySize, size_t NumFunctions>
	struct Stencils {
		using LoadedStencilsT = LoadedStencils<BinarySize, NumFunctions>;

		std::array<char, BinarySize> binary;  // why c++, why char
		LLVM_nm_data                 functions[NumFunctions];

		LoadedStencilsT load() const;
	};

	template<size_t BinarySize, size_t NumFunctions>
	struct LoadedStencils {
		using StencilsT = Stencils<BinarySize, NumFunctions>;

		static LoadedStencils load(StencilsT stencils) {
			return LoadedStencils{ .stencils = std::move(stencils),
				                   .dynlib   = DynamicLibrary::load(stencils.binary) };
		}

		std::span<const std::byte> stencil_binary(size_t index) const {
			return stencil_binary(stencils.functions[index]);
		}

		std::span<const std::byte> stencil_binary(const LLVM_nm_data& func_data) const {
			auto begin = reinterpret_cast<const std::byte*>(dynlib.findSymbol(func_data.name));
			return std::span(begin, begin + func_data.size);
		}

		auto& functions() const {
			return stencils.functions;
		}

		std::span<const std::byte> binary() const {
			return std::span((const std::byte*)stencils.binary.data(), stencils.binary.size());
		}

		StencilsT      stencils;
		DynamicLibrary dynlib;
	};

	template<size_t BinarySize, size_t NumFunctions>
	inline LoadedStencils<BinarySize, NumFunctions> Stencils<BinarySize, NumFunctions>::load() const {
		return LoadedStencilsT::load(*this);
	}
}
