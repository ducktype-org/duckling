// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file emit.hpp
 * @brief Writes a lowered module as Duckling source.
 */
#pragma once

#include "dk_model.hpp"

#include <string>
#include <vector>

namespace c_import {

	/**
	 * @brief The bindings module: `banner` as leading comments, the skipped declarations, then
	 * one `extern("C")` block with the classes and function declarations, then the constants
	 * and generated functions.
	 */
	std::string emitModule(const DkModule& module, const std::vector<std::string>& banner);

	/**
	 * @brief A module checking every layout of `module` against the one clang reported.
	 *
	 * It defines `fun c_layout_mismatch() -> i64`, returning 0 when every class matches, and
	 * otherwise 1 + the index into `DkModule::layouts` of the first one that does not.
	 * @param bindings_import import path of the bindings module, e.g. `sdl3.generated.sdl3`.
	 */
	std::string emitLayoutCheck(
		const DkModule&                 module,
		const std::vector<std::string>& banner,
		const std::string&              bindings_import
	);

}
