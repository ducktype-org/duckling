#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>

#include "../pst_ref.hpp"
#include "../hout/hout.hpp"
#include "../lookup_result.hpp"


namespace compiler::helios {

	/**
	 * @brief SymbolKind stores general kind/type of a symbol.
	 */
	enum class SymbolKind {
		Basic,
		Namespace,
		Function,
		CompilationUnit,
		Const,
		Struct,
		Alias,

		TestSymbol,
		// ...
	};

	// Such functions can probably be just functions:
	base::StrId name(SymID);
	SymbolKind kind(SymID);

	struct KeyOf_QuerySymbolOfSTMT {
		ScopeID scope;
		PstRef<pst::Stmt> stmt;
	};

	/**
	 * @brief Construct a new declare query object
	 */
	DECLARE_QUERY(QuerySymbolOfSTMT, KeyOf_QuerySymbolOfSTMT, SymID);

	// @TODO str
	DECLARE_QUERY(QueryLookupIn, SymID, LookupResult);

	// @TODO: get type from type-system
	struct Type {};
	DECLARE_QUERY(QueryTypeOF, SymID, Type);

	// @TODO: query symbol value (some CTV?) for execution somewhere
	// There is a good chance that logic behind it will be elsewhere
	// but HELIOS does need to somehow access at least some results of comp-time evaluation

	// @TODO: some proper hout type
	struct SomeHOUT {};
	/**
	 * @brief This query is effectively responsible for compilation of symbols.
	 * @TODO: is it recursive?
	 */
	DECLARE_QUERY(QueryHOUT, SymID, base::Optional<SomeHOUT>);


	// @TODO: dealias query

}