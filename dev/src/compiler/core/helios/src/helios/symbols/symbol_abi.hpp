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

		/**
		 * @brief The linked symbol name set with `@c_symbol_name("<name>")`.
		 * Empty when the symbol links under its own name.
		 */
		base::Optional<base::StrID> symbol_name;
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

	/**
	 * @brief ABI of the global `main` function.
	 *
	 * Like `CAbi`, the name is not mangled and the function is lowered with the C calling
	 * convention, so the C runtime can call it. Unlike `CAbi`, the declaration is not
	 * validated against the C ABI.
	 */
	struct MainAbi final {};

	using SymbolABI = std::variant<DefaultAbi, CAbi, DVMAbi, MainAbi>;

	/**
	 * @brief Simple wrapper on a SymbolABI.
	 */
	struct ABIWrapper final {
	private:
		SymbolABI value;

	public:
		/**
		 * @brief Get the ABI without checking that the symbol is representable in it.
		 */
		[[nodiscard]] const SymbolABI& withoutValidation() const { return value; }

		ABIWrapper(SymbolABI val): value(val) {}

		[[nodiscard]] query::QResult<CRef<SymbolABI>> withValidation(
			query::Context&                                                           ctx,
			std::variant<CRef<HOUTFunctionDeclaration>, CRef<tsh::ClassAbstractType>> val
		) const;
	};

	using QuerySymbolABI_Result = query::QResult<ABIWrapper>;

	/**
	 * @brief Get the ABI of the HELIOS symbol ID.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QuerySymbolABI, SymID, CRef<QuerySymbolABI_Result>, ({}));
}
