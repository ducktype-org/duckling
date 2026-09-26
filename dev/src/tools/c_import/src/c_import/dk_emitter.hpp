#pragma once

#include "c_decls.hpp"

#include <string>
#include <vector>

namespace c_import {

	struct EmitterInput final {
		TranslationUnitModel model;
		/** Headers named on the command line, reproduced in the banner. */
		std::vector<std::string> headers;
		/** Raw clang arguments, reproduced in the banner so output can be regenerated. */
		std::vector<std::string> clang_args;
	};

	/** Renders the bindings module. Deterministic: equal input gives byte-identical output. */
	std::string emitBindings(const EmitterInput& input);

	struct SplitModule final {
		/** Module name, which is also the file stem. */
		std::string name;
		std::string contents;
	};

	/**
	 * @brief Renders one bindings module per origin header.
	 *
	 * A module that names a record belonging to another one imports it. A sibling module has
	 * to be reached through the package name; the import then binds its last segment, which is
	 * what the `using` names.
	 * The dependencies cannot form a cycle: a record has to be complete to be named, and an
	 * incomplete one degrades to `cptr u8` instead of being named at all.
	 */
	std::vector<SplitModule> emitSplitBindings(
		const EmitterInput& input, const std::string& package_name
	);

	/**
	 * @brief Renders the module that gathers every split module under one name.
	 *
	 * @note `using` binds locally rather than re-exporting, so until re-exports are supported
	 *       the split modules have to be imported individually.
	 */
	std::string emitAggregateModule(
		const std::string& package_name, const std::vector<SplitModule>& modules
	);

	/** Renders the root module file, which exists only so the directory forms a module. */
	std::string emitRootModule(const std::string& package_name);

}
