/**
 * @file hout.hpp
 * @note This file is a placeholder for HOUT structures
 * that will be added in the future.
 */

#pragma once

#include "../scope_symbol_id.hpp"
#include <base/string_id.hpp>
#include <vector>

namespace compiler::helios {

	namespace code {
		// Forward declaration:
		struct CodeBlock;
	}

	/**
	 * @brief placeholder for code that can execute (expressions, function body, etc)
	 */
	struct HOUTCode {
		// @NOTE: as of right now HOUTCode contains shared ptr, to avoid a lot of boilerplate, and
		// copying
		std::shared_ptr<const code::CodeBlock> body;
	};

	/**
	 * @brief placeholder for functions, methods, etc
	 */
	struct HOUTFunction {
		// @TODO:
		// - arguments
		// - return values
		// - flags like "pure", "thread safe", "shared-thread-function", etc

		// @TODO: decide if HOUT functions should contain its HELIOS SymID

		base::StrId original_name;

		HOUTCode body;

		[[nodiscard]]
		std::string debugPrint() const;
	};

	/**
	 * @brief Represents a constant
	 * @note: this is a mock
	 * @TODO: make it represent more general stuff
	 */
	struct HOUTGlobalData {
		// @TODO: types

		// @TODO: decide if HOUT functions global data contain its HELIOS SymID
		// Currently it is here for pretty printing
		SymID helios_symbol;

		base::StrId original_name;

		// @TODO: CTV from TS:
		i64 value;

		[[nodiscard]]
		std::string debugPrint() const;
	};

	/**
	 * @brief Structure representing single HOUTUnit
	 */
	struct HOUTUnit {
		// all first class citizens of module should be here:
		// * types (in some way?)
		// * required baked template list?
		// * functions / methods / macros
		// * defined templates
		// * vector/references to hout of submodules? -- not necessarily needed
		// * what else?

		std::vector<HOUTGlobalData> glob_data;

		std::vector<HOUTFunction> functions;

		[[nodiscard]]
		std::string debugPrint() const;
	};
}
