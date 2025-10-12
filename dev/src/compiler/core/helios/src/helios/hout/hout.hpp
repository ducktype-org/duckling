/**
 * @file hout.hpp
 * @note This file is a placeholder for HOUT structures
 * that will be added in the future.
 */

#pragma once

#include "../scope_symbol_id.hpp"
#include "elements/expr.hpp"  // IWYU pragma: export

#include <helios/ctv/ctv.hpp>
#include <typesystem/higher/types.hpp>

#include <base/box.hpp>
#include <base/string_id.hpp>

#include <query_framework/query_int.hpp>

#include <memory>
#include <variant>
#include <vector>

namespace compiler::helios {

	// for friend:
	struct ImplementationOf_QueryDeclOfFun;

	namespace houtgen {
		struct ImplementationOf_QueryImplicitClassConstructor;
	}

	namespace code {
		// Forward declaration:
		struct CodeBlock;
		struct Parameter;
	}

	/**
	 * @brief Storage of information coming from function declaration without processing its body.
	 */
	struct HOUTFunctionDeclaration final {
		// @TODO: decide if HOUT functions declarations should contain its HELIOS SymID
		// - flags like "pure", "thread safe", "shared-thread-function", etc

		HOUTFunctionDeclaration() = delete;

		HOUTFunctionDeclaration(const HOUTFunctionDeclaration&) = delete;
		HOUTFunctionDeclaration(HOUTFunctionDeclaration&&)      = default;

		/**
		 * @note it is used for hashes
		 * @note For now, it works,
		 * but in the future with generics, and templates it might not
		 * We might want to add actual hash?
		 */
		SymID original_symbol;

		base::StrID original_name;

		tsh::SymbolType<> return_type;

		std::vector<code::Parameter> parameters;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

		[[nodiscard]]
		std::string debugPrint() const;

	private:
		HOUTFunctionDeclaration(
			SymID symbol, tsh::SymbolType<> ret_type, std::vector<code::Parameter> parameters
		);
		friend ImplementationOf_QueryDeclOfFun;
		friend houtgen::ImplementationOf_QueryImplicitClassConstructor;
	};

	/**
	 * @brief HOUT representation for function, etc. It is declaration extended by function content.
	 * @note it should be used for all function-like entities (macros, methods, etc.)
	 */
	struct HOUTFunction final {
	private:
		HOUTFunction(
			CRef<HOUTFunctionDeclaration>, const std::shared_ptr<const code::CodeBlock>& body
		);
		friend struct ImplementationOf_QueryCodeOfFun;
		friend houtgen::ImplementationOf_QueryImplicitClassConstructor;

	public:
		HOUTFunction() = delete;

		CRef<HOUTFunctionDeclaration> declaration;

		/**
		 * @note use of shared_ptr's is intentional, as they
		 * work well for incomplete types, and fit the use case.
		 * In the future we might optimize it to single (or zero) shared_ptr, but
		 * that will require some boilerplate.
		 */
		std::shared_ptr<const code::CodeBlock> body;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

		[[nodiscard]]
		std::string debugPrint() const;

	private:
		/**
		 * Construct a HOUT Function object from a SymID which appears in the PST.
		 * @param symbol The symbol of the function.
		 * @param ctx The query context to resolve the function's properties.
		 */
		HOUTFunction(SymID symbol, query::Context& ctx);
	};

	enum class HOUTGlobalDataType { Constant, Variable };

	struct HOUTGlobalConst final {
		CompileTimeValue value;
	};

	struct HOUTGlobalVariable final {
		// We cannot use Box<code::Expr> here because we use AUTO_CACHE_COPY,
		// and the HOUTUnit is copied during runtime. Also, the problem with the
		// copy constructor will go away once we pass HOUT expressions around as
		// references.
		// @TODO: Make this better.
		std::shared_ptr<Box<code::Expr>> initial_value;  ///< The initial value of the variable.
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
	struct HOUTGlobalData final {
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
