#pragma once

#include "dynamic_linker.hpp"
#include "relocations.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

namespace vm::jit {
	struct LLVM_nm_data {
		const char*              name;
		const char*              type;
		int                      place;
		int                      size;
		std::vector<StencilHole> to_patch   = {};
		std::vector<StencilHole> relocation = {};
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

		auto stencil_binary(this auto&& self, size_t index) {
			return self.stencil_binary(self.stencils.functions[index]);
		}

		std::span<const std::byte> stencil_binary(const LLVM_nm_data& func_data) const {
			auto begin = reinterpret_cast<const std::byte*>(dynlib.findSymbol(func_data.name));
			return std::span(begin, begin + func_data.size);
		}

		std::span<std::byte> stencil_binary(const LLVM_nm_data& func_data) {
			auto begin = reinterpret_cast<std::byte*>(dynlib.findSymbol(func_data.name));
			return std::span(begin, begin + func_data.size);
		}

		auto& functions() const { return stencils.functions; }

		std::span<const std::byte> binary() const {
			return std::span((const std::byte*) stencils.binary.data(), stencils.binary.size());
		}

		void relocate(const LLVM_nm_data& func_data, std::byte* new_address) {
			auto binary = stencil_binary(func_data);
			std::ranges::copy(binary, new_address);

			for (const StencilHole& hole: func_data.relocation)
				hole.relocate(binary.data(), new_address);
		}

		StencilsT      stencils;
		DynamicLibrary dynlib;
	};

	template<size_t BinarySize, size_t NumFunctions>
	inline LoadedStencils<BinarySize, NumFunctions> Stencils<BinarySize, NumFunctions>::load() const {
		return LoadedStencilsT::load(*this);
	}
}
