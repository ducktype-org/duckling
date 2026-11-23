// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file generate.hpp
 * @brief Reads C headers and produces the text of the Duckling modules binding them.
 */
#pragma once

#include "dk_model.hpp"
#include "lower.hpp"
#include "tu_reader.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace c_import {

	struct GenerateOptions final {
		ReadOptions  read;
		LowerOptions lower;
		/// Import path of the bindings module, used by the layout check module.
		std::string bindings_import;
		/// Lines put, as comments, at the top of every generated module.
		std::vector<std::string> banner;
	};

	struct Generated final {
		/// Compiler errors of the headers; nothing else is set when there are any.
		std::vector<std::string> errors;
		DkModule                 module;
		std::string              target_triple;
		std::size_t              non_numeric_macros = 0;
		/// The bindings module.
		std::string bindings;
		/**
		 * @brief A module comparing every class's layout with clang's, for the target the headers
		 * were read for; it defines `c_layout_mismatch() -> i64`.
		 */
		std::string layout_check;
	};

	/// @note clang's resource directory defaults to the one duckbind was built with.
	Generated generate(GenerateOptions options);

	/**
	 * @brief `triple` without its vendor (`x86_64-pc-linux-gnu` and `x86_64-linux-gnu` both become
	 * `x86_64-unknown-linux-gnu`), which does not change the ABI. Apple's is kept.
	 */
	std::string abiTriple(std::string_view triple);

}
