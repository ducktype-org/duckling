#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

#include "../pst_ref.hpp"
#include "../hout/hout.hpp"
#include "../lookup_result.hpp"

#include <base/string_id.hpp>
#include <typesystem/typesystem.hpp>

namespace compiler::helios {

	/**
	 * @brief SymbolKind stores general kind/type of a symbol.
	 */
	enum class SymbolKind {
		Basic,
		Namespace,
		Function,
		Const,
		Struct,
		Alias,
		Using,

		// ...
	};

	// Following functions are left as functions:
	// in the future once Query System implementation will mature
	// they will probably be have to be converted into queries
	// from DefID to given Data:

	/**
	 * @return is symbol a wildcard symbol (e.g. using a.*)
	 */
	bool isWildcard(SymID);

	/**
	 * @return name of given symbol
	 */
	base::StrId name(SymID);

	/**
	 * @return kind of given symbol
	 */
	SymbolKind kind(SymID);

	/**
	 * @return scope given symbol was defined within
	 */
	ScopeID scope(SymID);

	struct KeyOf_QuerySymbolOfSTMT {
		/**
		 * @brief scope to create symbol in
		 *
		 * @TODO: is this needed? -- now you can create two symbols from the same pst element.
		 * It might be better to derive scope structure directly from PST structure.
		 */
		ScopeID scope;

		/**
		 * @brief Statement to change to symbol
		 */
		PstRef<pst::Stmt> stmt;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_QuerySymbolOfSTMT&) const = default;
	};

	/**
	 * @brief Returns symbols associated with given element in PST
	 */
	DECLARE_QUERY(QuerySymbolOfSTMT, KeyOf_QuerySymbolOfSTMT, SymID);

	struct KeyOf_LookupInSymbol {
		/**
		 * @brief Symbol to lookup in
		 */
		SymID symbol;

		/**
		 * @brief Name to lookup
		 */
		base::StrId name;

		/**
		 * @brief Should wildcards be included in lookup
		 */
		bool follow_wildcards;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_LookupInSymbol&) const = default;
	};

	/**
	 * @brief Returns result of "symbol.name" operation.
	 */
	DECLARE_QUERY(QueryLookupInSymbol, KeyOf_LookupInSymbol, const LookupResult&);

	/**
	 * @brief Returns type of given symbol.
	 * @note: not implemented yet
	 */
	DECLARE_QUERY(QueryTypeOF, SymID, ts::TypeInfo);

	// @TODO: query symbol value (some CTV?) for execution somewhere
	// There is a good chance that logic behind it will be elsewhere
	// but HELIOS does need to somehow access at least some results of comp-time evaluation

	// @TODO: some proper hout type
	struct SomeHOUT {};

	/**
	 * @brief Returns HOUT of given symbol
	 * @note This query is effectively responsible for compilation of symbols.
	 * @note not yet implemented
	 * @TODO: is it recursive?
	 */
	DECLARE_QUERY(QueryHOUT, SymID, base::Optional<SomeHOUT>);


	/**
	 * @brief Retuns scope to lookup in
	 * when looking up in given symbol.
	 *
	 * @note For HELIOS internal use only
	 * @note It is a partial Query it for example does not necessary
	 */
	DECLARE_QUERY(QueryLinkedScope, SymID, ScopeID);


	/**
	 * A query that returns an "absolute path" to the symbol without aliases.
	 */
	DECLARE_QUERY(QueryDealias, SymID, const SymbolList&);

	/**
	 * Calculates a value of a contant.
	 */
	DECLARE_QUERY(QueryConstValueOf, SymID, i32);

	namespace rpn {
		struct Operator {
			base::StrId oper_id;
		};

		struct Identifier {
			SymbolList symbol_list;
		};

		struct NumLiteral {
			// This can be replaced with type-system type and value
			base::StrId num_id;
		};

		struct KeywordValue {
			rift_def::Keyword keyword;
		};

		using ExprElem = std::variant<Operator, Identifier, NumLiteral, KeywordValue>;

		struct KeyOf_ExtensionMakeRPN {
			const std::vector<pst::Expr::ExprElem>& expr;
			ScopeID                                 expr_scope;
		};

		QUERY_EXTENSION(ExtensionMakeRPN, KeyOf_ExtensionMakeRPN, std::vector<ExprElem>);

		struct KeyOf_ExtensionRPNEval {
			ExprElem a;
			Operator op;
			ExprElem b;
			ScopeID  expr_scope;
		};

		QUERY_EXTENSION(ExtensionRPNEval, const KeyOf_ExtensionRPNEval&, ExprElem);

		struct KeyOf_ExtensionRPNValue {
			ExprElem expr;
			ScopeID  expr_scope;
		};

		QUERY_EXTENSION(ExtensionRPNValue, const KeyOf_ExtensionRPNValue&, i32);
	}
}
