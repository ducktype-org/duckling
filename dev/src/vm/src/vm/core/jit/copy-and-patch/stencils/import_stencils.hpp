// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../../../native/dynamic_library.hpp"
#include "relocations.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vm::jit::cnp {
	using vm::native::DynamicLibrary;

	/**
	 * @brief All informations used for future patching of the copied stencil.
	 */
	struct StencilData final {
		const char*              name;
		usize                    place;
		usize                    size;
		std::vector<StencilHole> to_patch = {};

		/**
		 * @brief Patch a stencil into a given address.
		 */
		void patch(byte* new_address, auto patch_values) const {
			for (auto hole: to_patch) hole.patch(new_address, patch_values(hole.value));
		}
	};

	/**
	 * @brief Stencils that have been dynamically linked (had their dependencies resolved).
	 */
	template<usize BinarySize, usize NumFunctions>
	struct LoadedStencils;

	template<usize BinarySize, usize NumFunctions>
	struct Stencils final {
		using LoadedStencilsT = LoadedStencils<BinarySize, NumFunctions>;

		std::array<byte, BinarySize>          stencils_binary;
		std::array<StencilData, NumFunctions> stencils_data;

		/**
		 * @brief Dynamically link the stored stencils, resolving their dependencies.
		 */
		[[nodiscard]] std::expected<LoadedStencilsT, std::string> load() &&;
	};

	template<usize BinarySize, usize NumFunctions>
	struct LoadedStencils final {
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
		[[nodiscard]] std::span<const byte> stencilsBinary(const StencilData& stencil_data) const {
			auto begin = dynlib.findSymbol(stencil_data.name);
			return std::span(begin, begin + stencil_data.size);
		}

		[[nodiscard]] auto& stencilsData() const { return stencils.stencils_data; }

		/**
		 * @brief Copy a stencil into a given address.
		 */
		byte* relocate(const StencilData& stencil_data, byte* new_address) {
			auto binary = stencilsBinary(stencil_data);
			std::ranges::copy(binary, new_address);
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
