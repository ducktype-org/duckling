#pragma once


#include <helios/symbols/symbol_id.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <string_id/string_id.hpp>

#include <variant>

namespace compiler::helios {

	/**
	 * @brief Symbol ABI information.
	 * For example when symbol is has `extern("C")` specifier it will have C ABI.
	 */
	struct CAbi {
		/**
		 * @brief Name of the library where the symbol is located and (in the future dynamically)
		 * linked from. e.x. `extern("C" "mylib")`
		 */
		base::Optional<base::StrID> library;

		/**
		 * @brief Number of fixed parameters of a variadic C function.
		 * Empty if a function is not variadic.
		 */
		base::Optional<u64> fixed_params;
	};

	struct DefaultAbi final {};

	/**
	 * @brief ABI of symbols declared with `extern("DVM")`.
	 *
	 * Behaves like `DefaultAbi` (no C-ABI type requirements, so pointers stay `manyptr`
	 * instead of being lowered to C pointers, which the DVM does not support) but, like
	 * `CAbi`, the name is not mangled. This lets `fundecl`s keep the plain name the DVM
	 * builtin registry (`vm::builtins::getBuiltinFunctions`) looks up.
	 */
	struct DVMAbi final {};

	using SymbolABI = std::variant<DefaultAbi, CAbi, DVMAbi>;

	using QuerySymbolABI_Result = query::QResult<SymbolABI>;

	/**
	 * @brief Get the ABI of the HELIOS symbol ID.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QuerySymbolABI, SymID, CRef<QuerySymbolABI_Result>, ({}));
}
