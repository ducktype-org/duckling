/**
 * @file hout.hpp
 * @note This file is a placeholder for HOUT structures
 * that will be added in the future.
 */

#pragma once

#include "../scope_symbol_id.hpp"
#include "elements/expr.hpp"  // IWYU pragma: export

#include <query_framework/query_int.hpp>
#include <typesystem/higher/types.hpp>

#include <base/box.hpp>
#include <base/string_id.hpp>

#include <memory>
#include <variant>
#include <vector>

namespace compiler::helios {

	// for friend:
	struct ImplementationOf_QueryCodeOFFun;

	namespace code {
		// Forward declaration:
		struct CodeBlock;
		struct Parameter;
		struct Expr;
	}

	/**
	 * @brief Storage for heavy function data,
	 * in a way that makes it cheap to copy, since
	 * we want HOUTFunction to be a copyable object.
	 * @note use of shared_ptr's is intentional, as they
	 * work well for incomplete types, and fit the use case.
	 * In the future we might optimize it to single (or zero) shared_ptr, but
	 * that will require some boilerplate.
	 */
	struct HOUTFunctionContent {
		std::shared_ptr<const std::vector<code::Parameter>> parameters;
		std::shared_ptr<const code::CodeBlock>              body;
	};

	/**
	 * @brief placeholder for functions, methods, etc
	 */
	struct HOUTFunction {
		// @TODO:
		// - flags like "pure", "thread safe", "shared-thread-function", etc

		// @TODO: decide if HOUT functions should contain its HELIOS SymID

		HOUTFunction() = delete;

		HOUTFunction(const HOUTFunction&) = default;
		HOUTFunction(HOUTFunction&&)      = default;

		/**
		 * @note it is used for hashes, and == only
		 * @note For now it works,
		 * but in the future with generics, and templates it might not
		 * We might want to add actual hash?
		 */
		SymID original_symbol;

		base::StrID original_name;

		HOUTFunctionContent content;

		tsh::FunctionAbstractType type;

		/**
		 * @brief Lifetime scope, thats higher
		 * then any lifetime scope in the function (including parameters)
		 */
		helios::ScopeID top_lifetime_scope;

		[[nodiscard]]
		std::string debugPrint() const;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

		bool operator==(const HOUTFunction& oth) const {
			return original_symbol == oth.original_symbol;
		}

	private:
		/**
		 * Construct a HOUT Function object.
		 * @param symbol The symbol of the function.
		 * @param ctx The query context to resolve the function's properties.
		 */
		HOUTFunction(SymID symbol, query::Context& ctx);

		friend ImplementationOf_QueryCodeOFFun;
	};

	enum class HOUTGlobalDataType { Constant, Variable };

	struct HOUTGlobalConst {
		// @TODO: CTV from TS:
		i64 value;
	};

	struct HOUTGlobalVariable {
		std::shared_ptr<Box<code::Expr>> initial_value; ///< The initial value of the variable.
	};

	/**
	 * @brief Represents global data in the HOUT module.
	 * 
	 * This structure is used to store information about global constants and variables
	 * in the HOUT representation of a module. It supports two types of global data:
	 * constants and variables, distinguished by the `data_type` field.
	 * 
	 * - Constants are represented using `HOUTGlobalConst`.
	 * - Variables are represented using `HOUTGlobalVariable`.
	 * 
	 * The `value` field is a variant that holds either a constant or a variable,
	 * depending on the `data_type`. This structure also includes metadata such as
	 * the original name, symbol ID, and type of the global data.
	 */
	struct HOUTGlobalData {
		// @TODO: decide if HOUT functions global data contain its HELIOS SymID
		// Currently it is here for pretty printing
		SymID helios_symbol;

		base::StrID original_name;

		HOUTGlobalDataType data_type;

		std::variant<HOUTGlobalConst, HOUTGlobalVariable> value;

		tsh::SymbolType<> type;

		HOUTGlobalData(SymID symbol, query::Context& ctx, HOUTGlobalDataType data_type);

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
