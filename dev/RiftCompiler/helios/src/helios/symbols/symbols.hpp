#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

#include "../pst_ref.hpp"
#include "../hout/hout.hpp"
#include "../lookup_result.hpp"
#include "base/string_id.hpp"

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
		Using,

		TestSymbol,
		// ...
	};

	// Such functions can probably be just functions:
	base::StrId name(SymID);
	SymbolKind  kind(SymID);
	ScopeID     scope(SymID);

	struct KeyOf_QuerySymbolOfSTMT {
		// @TODO: is this needed? -- it ads inconsistency
		ScopeID scope;

		PstRef<pst::Stmt> stmt;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_QuerySymbolOfSTMT&) const = default;
	};

	/**
	 * @brief Construct a new declare query object
	 */
	DECLARE_QUERY(QuerySymbolOfSTMT, KeyOf_QuerySymbolOfSTMT, SymID);


	struct KeyOf_LookupIn {
		SymID symbol;
		base::StrId name;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_LookupIn&) const = default;
	};
	DECLARE_QUERY(QueryLookupIn, KeyOf_LookupIn, LookupResult);

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
