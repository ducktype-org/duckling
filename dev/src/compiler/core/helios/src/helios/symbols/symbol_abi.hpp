#pragma once


#include <helios/scope_symbol_id.hpp>

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
	};

	struct DefaultAbi final {};

	using SymbolABI = std::variant<DefaultAbi, CAbi>;

	using QuerySymbolABI_Result = query::QResult<SymbolABI>;

	/**
	 * @brief Get the ABI of the HELIOS symbol ID.
	 *
	 * @ingroup query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QuerySymbolABI, SymID, CRef<QuerySymbolABI_Result>, ({}));
}
