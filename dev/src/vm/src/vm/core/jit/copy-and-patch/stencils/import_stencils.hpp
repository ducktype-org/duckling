#pragma once

#include "dynamic_library.hpp"
#include "relocations.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vm::jit::cnp {

	/**
	 * @brief All informations used for future patching of the copied stencil.
	 */
	struct StencilData {
		const char*              name;
		const char*              type;
		usize                    place;
		usize                    size;
		std::vector<StencilHole> to_patch   = {};
		std::vector<StencilHole> relocation = {};
	};

	/**
	 * @brief Stencils that have been dynamically linked (had their dependencies resolved).
	 */
	template<usize BinarySize, usize NumFunctions>
	struct LoadedStencils;

	template<usize BinarySize, usize NumFunctions>
	struct Stencils {
		using LoadedStencilsT = LoadedStencils<BinarySize, NumFunctions>;

		std::array<byte, BinarySize>          stencils_binary;
		std::array<StencilData, NumFunctions> stencils_data;

		/**
		 * @brief Dynamically link the stored stencils, resolving their dependencies.
		 */
		[[nodiscard]] std::expected<LoadedStencilsT, std::string> load() &&;
	};

	template<usize BinarySize, usize NumFunctions>
	struct LoadedStencils {
		using StencilsT = Stencils<BinarySize, NumFunctions>;

		LoadedStencils()                                 = delete;
		LoadedStencils(const LoadedStencils&)            = delete;
		LoadedStencils& operator=(const LoadedStencils&) = delete;

		LoadedStencils(LoadedStencils&&)            = default;
		LoadedStencils& operator=(LoadedStencils&&) = default;
		~LoadedStencils()                           = default;

		/**
		 * @brief Dynamically link the stored stencils, resolving their dependencies.
		 */
		[[nodiscard]] static std::expected<LoadedStencils, std::string> load(StencilsT&& stencils) {
			return DynamicLibrary::fromMemory(stencils.stencils_binary)
			    .transform([&](DynamicLibrary loaded_library) {
					return LoadedStencils{ std::move(stencils), std::move(loaded_library) };
				});
		}

		/**
		 * @brief Get the span of a stencil.
		 */
		[[nodiscard]] std::span<const byte> stencilBinary(const StencilData& stencil_data) const {
			auto begin = dynlib.findSymbol(stencil_data.name);
			return std::span(begin, begin + stencil_data.size);
		}

		[[nodiscard]] auto& stencilsData() const { return stencils.stencils_data; }

		/**
		 * @brief Copy and patch a stencil into a given address.
		 */
		byte* relocate(const StencilData& stencil_data, byte* new_address) {
			auto binary = stencilBinary(stencil_data);
			std::ranges::copy(binary, new_address);

			for (const StencilHole& hole: stencil_data.relocation)
				hole.relocate(binary.data(), new_address);
			return new_address + binary.size_bytes();
		}

	private:
		StencilsT      stencils;
		DynamicLibrary dynlib;

		LoadedStencils(StencilsT&& in_stencils, DynamicLibrary&& in_dynlib):
			  stencils{ std::move(in_stencils) },
			  dynlib{ std::move(in_dynlib) } {}
	};

	template<usize BinarySize, usize NumFunctions>
	inline std::expected<LoadedStencils<BinarySize, NumFunctions>, std::string> Stencils<
		BinarySize,
		NumFunctions>::load() && {
		return LoadedStencilsT::load(std::move(*this));
	}
}
