#pragma once

#include <dlfcn.h>
#include <unistd.h>

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
	struct LoadedStencils: Stencils<BinarySize, NumFunctions> {
		using StencilsT = Stencils<BinarySize, NumFunctions>;

		int   lib_fd;
		void* lib_handle;

		LoadedStencils(int fd, void* handle, StencilsT stencils):
			  StencilsT{ std::move(stencils) },
			  lib_fd{ fd },
			  lib_handle{ handle } {}

		static LoadedStencils load(StencilsT stencils);

		template<class T = void>
		T* findSymbol(const char* name) const;

		std::span<const std::byte> stencil_binary(size_t index) const {
			return stencil_binary(StencilsT::functions[index]);
		}

		std::span<const std::byte> stencil_binary(const LLVM_nm_data& func_data) const {
			auto begin = reinterpret_cast<const std::byte*>(findSymbol(func_data.name));
			return std::span(begin, begin + func_data.size);
		}
	};

	template<size_t BinarySize, size_t NumFunctions>
	inline LoadedStencils<BinarySize, NumFunctions> Stencils<BinarySize, NumFunctions>::load() const {
		return LoadedStencilsT::load(*this);
	}
}
#include "import_stencils-unix.cpp"
