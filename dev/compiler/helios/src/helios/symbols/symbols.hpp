/**
 * @file symbols.hpp
 * @brief This file defines Queries responsible for creation of Symbols and operations on them.
 */
#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>

#include "../lookup_result.hpp"
#include "../helios_errors.hpp"
#include "../helios_result.hpp"
#include "base/box.hpp"
#include <base/unique_pointer.hpp>
#include <typesystem/higher/type_info.hpp>
#include <helios/scopes/scopes.hpp>
#include <helios/scope_symbol_id.hpp>

#include <base/string_id.hpp>
#include <typesystem/higher/type_info.hpp>

namespace compiler::helios {

	/**
	 * @brief Stores general kind/type of a symbol.
	 */
	enum class SymbolKind {
		Namespace,
		Function,
		Const,
		Class,
		Alias,
		Using,
		Variable,
		Import,

		// Class Symbols
		Method,
		Field,
		Constructor,
		Destructor,
		// ...
	};

	// Following functions are left as functions (instead of beeing a query):
	// in the future once Query System implementation will mature
	// they will probably be have to be converted into queries
	// from DefID/PstID to appropriate data:

	/**
	 * @return is given symbol a wildcard symbol (e.g. using a.*)
	 */
	bool isWildcard(SymID);

	/**
	 * @return name of the symbol
	 */
	base::StrID name(SymID);

	/**
	 * @return kind of the symbol
	 */
	SymbolKind kind(SymID);

	/**
	 * @return scope that given symbol was defined within
	 */
	ScopeID scope(SymID);

	/**
	 * @return Pst element symbol was created from
	 */
	MCRef<pst::Stmt> stmt(SymID);

	struct KeyOf_QuerySymbolOfSTMT {
		/**
		 * @brief scope to create symbol in
		 *
		 * @TODO: is this needed? -- now you can create two symbols from the same pst element.
		 * It might be better to derive scope structure directly from PST structure.
		 * @TODO: scopes are already derived like this, it should now be deleted
		 */
		ScopeID scope;

		/**
		 * @brief Statement to change to symbol
		 */
		MCRef<pst::Stmt> stmt;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_QuerySymbolOfSTMT&) const = default;
	};

	/**
	 * @brief Query symbols associated with given element in PST
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
		base::StrID name;

		/**
		 * @brief Should wildcards be included in lookup
		 */
		bool follow_wildcards;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_LookupInSymbol&) const = default;
	};

	/**
	 * @brief Query result of lookup of single name within the symbol.
	 * It essentially implements "symbol.name" operation.
	 */
	DECLARE_QUERY(QueryLookupInSymbol, KeyOf_LookupInSymbol, CRef<LookupResult>);

	using QueryDealias_Result = errors::HResult<SymbolList, errors::Failed>;
	/**
	 * A query that returns an "absolute path" to the symbol without aliases.
	 */
	DECLARE_QUERY(QueryDealias, SymID, CRef<QueryDealias_Result>);

	using PotentialParsingErrors = std::
		variant<errors::SymbolNotFound, errors::Ambiguity, errors::InvalidExpr, errors::Failed>;
	/**
	 * Calculates a value of a constant.
	 */
	DECLARE_QUERY(QueryConstValueOf, SymID, CRef<errors::HResult<i64 COMMA errors::Failed>>)

	using ParseTypeFromExpr_Result = errors::HResult<tsh::TypeInfo, PotentialParsingErrors>;
	using QueryType_Result         = errors::HResult<tsh::TypeInfo, errors::Failed>;

	/**
	 * @brief Query type of the symbol.
	 */
	DECLARE_QUERY(QueryTypeOfSymbol, SymID, CRef<QueryType_Result>);

	/**
	 * @brief Query tsh::TypeInfo from a symbol definition (like class definition).
	 *
	 * Example:
	 * class T {
	 *	...
	 * }
	 * - Then we can use this query QueryTypeFromDefinition(T).
	 */
	DECLARE_QUERY(QueryTypeFromDefinition, SymID, CRef<QueryType_Result>);

	/**
	 * @brief Does QueryTypeOfSymbol and upon failing does QueryTypeFromDefinition.
	 */
	DECLARE_QUERY(QueryTypeOfSymbolOrDefinition, SymID, CRef<QueryType_Result>);

	/**
	 * @brief Struct returned by the `QueryClassSymbolData` query.
	 */
	struct ClassSymbolData {
		/**
		 * @brief Name of the class in the source code.
		 */
		base::StrID name;
		/**
		 * @brief Class'es declared methods.
		 */
		std::vector<SymID> methods;
		/**
		 * @brief Class'es declared constructors.
		 */
		std::vector<SymID> constructors;
		/**
		 * @brief Class'es declared destructor.
		 */
		base::Optional<SymID> destructor;
		/**
		 * @brief Class'es declared member variables.
		 */
		std::vector<SymID> members;
		/**
		 * @brief Class'es base class.
		 */
		base::Optional<tsh::TypeInfo> base;
		/**
		 * @brief Class'es implemented interfaces.
		 */
		std::vector<tsh::TypeInfo> implements;
	};

	using QueryClassSymbolData_Result = errors::HResult<ClassSymbolData, errors::Failed>;

	/**
	 * @brief Query all the information about a class definition.
	 * Panics if the given `SymID` is not a class.
	 * More information on `ClassSymbolData` in it's definition.
	 */
	DECLARE_QUERY(QueryClassSymbolData, SymID, CRef<QueryClassSymbolData_Result>)

	namespace code {
		struct Expr;
	}

	/**
	 * @brief Return Expr tree of HOUT of a expression assigned to a constant.
	 * @note This query is temporary and is used for testing only.
	 * @note type of this query is weird, but it will likely be refactored in expr-2.0 anyway
	 */
	DECLARE_QUERY(QueryHOUTExprTreeOfSym, SymID, CRef<errors::HResult<base::Box<code::Expr> COMMA errors::Failed>>);
}
