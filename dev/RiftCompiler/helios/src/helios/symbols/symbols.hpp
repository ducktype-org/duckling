#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

#include "../pst_ref.hpp"
#include "../hout/hout.hpp"
#include "../lookup_result.hpp"
#include "base/string_id.hpp"
#include "typesystem/type_info.hpp"


#include <typesystem/typesystem.hpp>

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
	bool        isWildcard(SymID);
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

	struct KeyOf_LookupInSymbol {
		SymID       symbol;
		base::StrId name;
		bool        follow_wildcards;

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const KeyOf_LookupInSymbol&) const = default;
	};

	DECLARE_QUERY(QueryLookupInSymbol, KeyOf_LookupInSymbol, const LookupResult&);

	DECLARE_QUERY(QueryTypeOF, SymID, ts::TypeInfo);

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


	/**
	 * @brief Partial function probably
	 */
	DECLARE_QUERY(QueryLinkedScope, SymID, ScopeID);


	/**
	 * A query that returns an "absolute path" to the symbol without aliases.
	 */
	DECLARE_QUERY(QueryDealias, SymID, SymbolList);
}
