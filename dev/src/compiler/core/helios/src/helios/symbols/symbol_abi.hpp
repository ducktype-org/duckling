#pragma once

#include <helios/scope_symbol_id.hpp>


#include <pst_parser/access.hpp>
#include <pst_parser/elements/hierarchy/statements/stmt_specifier.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <helios/helios_errors.hpp>

namespace compiler::helios {

	/**
	 * @brief Symbol ABI information.
	 * For example when symbol is has `extern("C")` specifier it will have C ABI.
	 */
	struct CAbi {
		base::Optional<base::StrID> library;
	};

	struct DefaultAbi final {};

	using SymbolABI = std::variant<DefaultAbi, CAbi>;

    using QuerySymbolABI_Result = query::QResult<SymbolABI, errors::Failed>;
	/**
	 * @brief Query symbols associated with given element in PST
	 */
	DECLARE_QUERY(QuerySymbolABI, SymID, CRef<QuerySymbolABI_Result>);
}