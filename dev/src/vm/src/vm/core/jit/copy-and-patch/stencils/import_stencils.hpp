#pragma once

#include "dynamic_linker.hpp"
#include "relocations.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace vm::jit::cnp {
	struct LLVM_nm_data {
		const char*              name;
		const char*              type;
		int                      place;
		int                      size;
		std::vector<StencilHole> to_patch   = {};
		std::vector<StencilHole> relocation = {};
	};

	template<usize BinarySize, usize NumFunctions>
	struct LoadedStencils;

	template<usize BinarySize, usize NumFunctions>
	struct Stencils {
		using LoadedStencilsT = LoadedStencils<BinarySize, NumFunctions>;

		std::array<byte, BinarySize>           binary;
		std::array<LLVM_nm_data, NumFunctions> functions;

		[[nodiscard]] LoadedStencilsT load() const;
	};

	template<usize BinarySize, usize NumFunctions>
	struct LoadedStencils {
		using StencilsT = Stencils<BinarySize, NumFunctions>;

		[[nodiscard]] static LoadedStencils load(StencilsT stencils) {
			return LoadedStencils{ .stencils = std::move(stencils),
				                   .dynlib   = DynamicLibrary::load(stencils.binary) };
		}

		[[nodiscard]] auto stencilBinary(this auto&& self, usize index) {
			return self.stencilBinary(self.stencils.functions[index]);
		}

		[[nodiscard]] std::span<const byte> stencilBinary(const LLVM_nm_data& func_data) const {
			auto begin = dynlib.findSymbol(func_data.name);
			return std::span(begin, begin + func_data.size);
		}

		[[nodiscard]] auto& functions() const { return stencils.functions; }

		[[nodiscard]] std::span<const byte> binary() const {
			return std::span((const byte*) stencils.binary.data(), stencils.binary.size());
		}

		void relocate(const LLVM_nm_data& func_data, byte* new_address) {
			auto binary = stencilBinary(func_data);
			std::ranges::copy(binary, new_address);

			for (const StencilHole& hole: func_data.relocation)
				hole.relocate(binary.data(), new_address);
		}

		StencilsT      stencils;
		DynamicLibrary dynlib;
	};

	template<usize BinarySize, usize NumFunctions>
	inline LoadedStencils<BinarySize, NumFunctions> Stencils<BinarySize, NumFunctions>::load() const {
		return LoadedStencilsT::load(*this);
	}
}
