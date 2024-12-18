/**
 * @file hout.hpp
 * @note This file is a placeholder for HOUT structures
 * that will be added in the future.
 */

#pragma once

#include "../scope_symbol_id.hpp"
#include <base/string_id.hpp>
#include <typesystem/higher/types.hpp>
#include <query_framework/query_int.hpp>
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
		// - flags like "pure", "thread safe", "shared-thread-function", etc

		// @TODO: decide if HOUT functions should contain its HELIOS SymID

		/**
		 * @note it is used for hashes, and == only
		 * @note For now it works,
		 * but in the future with generics, and templates it might not
		 * We might want to add actual hash?
		 */
		SymID original_symbol;

		// @TODO: store here list of parameters

		base::StrID original_name;

		HOUTCode body;

		tsh::FunctionInfo type;

		/**
		 * @brief Lifetime scope, thats higher
		 * then any lifetime scope in the function (including parameters)
		 */
		helios::ScopeID top_lifetime_scope;

		/**
		 * Construct a HOUT Function object.
		 * @param symbol The symbol of the function.
		 * @param ctx The query context to resolve the function's properties.
		 */
		HOUTFunction(SymID symbol, query::Context& ctx);

		[[nodiscard]]
		std::string debugPrint() const;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const HOUTFunction& oth) const {
			return original_symbol == oth.original_symbol;
		}
	};

	/**
	 * @brief Represents a constant
	 * @note: this is a mock
	 * @TODO: make it represent more general stuff
	 */
	struct HOUTGlobalData {
		// @TODO: decide if HOUT functions global data contain its HELIOS SymID
		// Currently it is here for pretty printing
		SymID helios_symbol;

		base::StrID original_name;

		// @TODO: CTV from TS:
		i64 value;

		tsh::TypeInfo type;

		HOUTGlobalData(SymID symbol, query::Context& ctx);

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
