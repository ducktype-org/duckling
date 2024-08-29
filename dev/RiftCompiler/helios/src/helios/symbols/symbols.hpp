/**
 * @file symbols.hpp
 * @brief This file defines Queries responsible for creation of Symbols and operations on them.
 */
#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>

#include "../pst_ref.hpp"
#include "../lookup_result.hpp"
#include <helios/scope_symbol_id.hpp>

#include <base/string_id.hpp>
#include <typesystem/type_info.hpp>

namespace compiler::helios {

	/**
	 * @brief Stores general kind/type of a symbol.
	 */
	enum class SymbolKind {
		Namespace,
		Function,
		Const,
		Struct,
		Alias,
		Using,
		Variable,
		Import
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
	base::StrId name(SymID);

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
	PstRef<pst::Stmt> stmt(SymID);

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
	 * @brief Query result of lookup of single name within the symbol.
	 * It essentially implements "symbol.name" operation.
	 */
	DECLARE_QUERY(QueryLookupInSymbol, KeyOf_LookupInSymbol, const LookupResult&);


	/**
	 * A query that returns an "absolute path" to the symbol without aliases.
	 */
	DECLARE_QUERY(QueryDealias, SymID, const SymbolList&);

	/**
	 * Calculates a value of a constant.
	 */
	DECLARE_QUERY(QueryConstValueOf, SymID, i32);

	/**
	 * RPN - Reverse Polish Notation.
	 * This namespace contains transformed `pst::ExprElem`s, but without groups and with
	 * looked-up symbols.
	 * These structs are used to form a RPN expression.
	 */
	namespace rpn {
		/**
		 * @brief Operator - any operator.
		 */
		struct Operator {
			/**
			 * @brief A string representing the operator.
			 */
			base::StrId oper_id;
		};

		/**
		 * @brief A not-yet looked up identifier.
		 */
		struct NamedIdentifier {
			/**
			 * @brief A name to lookup when needed..
			 */
			base::StrId symbol_name;
		};

		/**
		 * @brief A symbol identifier.
		 */
		struct Identifier {
			/**
			 * @brief A SymbolList returned by a lookup.
			 */
			SymbolList symbol_list;
		};

		/**
		 * @brief A numerical value.
		 */
		struct NumValue {
			/**
			 * @TODO: Replace it with TypeSystem's value.
			 * The value representation.
			 */
			base::StrId num_id;
		};

		/**
		 * @brief A keyword value, like `None`.
		 */
		struct KeywordValue {
			rift_def::Keyword keyword;
		};

		/**
		 * @brief A tuple call. It's meant as a information for the evaluator
		 * to take `num_elements` expressions from the stack as tuple elements.
		 */
		struct TupleConstructor {
			usize num_elements;
		};

		struct TupleType;
		struct Variant;

		using ExprElem = std::variant<
			Operator,
			NamedIdentifier,
			Identifier,
			NumValue,
			KeywordValue,
			TupleConstructor,
			TupleType,
			Variant>;

		/**
		 * @brief A constructed tuple. It differs from the TupleConstructor in a way that
		 * this is something created during RPN expression evaluation, not creation.
		 */
		struct TupleType {
			std::vector<ExprElem> elements;
		};

		/**
		 * @brief A variant constructed from other expressions (types).
		 */
		struct Variant {
			std::vector<ExprElem> elements;
		};

		struct KeyOf_RPNmakeRPN {
			/**
			 * @brief The expression to parse from pst.
			 */
			const std::vector<pst::Expr::ExprElem>& expr;
			/**
			 * @brief A scope that the expression was written.
			 */
			ScopeID expr_scope;
		};

		/**
		 * @brief RPN (postfix) expression with scope produced by makeRPN().
		 */
		struct RPNExpr {
			/**
			 * @brief Elements of the RPN expression.
			 */
			std::vector<ExprElem> elements;

			/**
			 * @brief Scope, where the expression was expressed in.
			 */
			ScopeID scope;
		};

		/**
		 * @brief Parses an expression from PST into RPN.
		 */
		RPNExpr makeRPN(query::Context&, KeyOf_RPNmakeRPN);

		struct KeyOf_evalOperator {
			/**
			 * @brief Symbol on the left.
			 */
			ExprElem a;
			/**
			 * @brief Operator to make a operation with.
			 */
			Operator op;
			/**
			 * @brief Symbol on the right.
			 */
			ExprElem b;
			/**
			 * @brief A scope, where the expression was written.
			 */
			ScopeID expr_scope;
		};

		/**
		 * @brief Evaluates an operation `a (op) b`.
		 */
		ExprElem evalOperator(query::Context&, const KeyOf_evalOperator&);

		struct KeyOf_parseValue {
			/**
			 * @brief The expression to parse.
			 */
			ExprElem expr;
			/**
			 * @brief A scope, where the expression was written.
			 */
			ScopeID expr_scope;
		};

		/**
		 * @TODO: Change this from i32 to typesystem's value.
		 * Parses a value from rpn::ExprElem.
		 *
		 * For example, if we pass here a rpn::NumLiteral(5), then it will return 5 or if we pass
		 * rpn::Identifier([C]), then a value of a C will be returned (if it's a constant).
		 */
		i32 parseValue(query::Context&, const KeyOf_parseValue&);

		/**
		 * @brief Evaluates RPN expression. Expects a single element to be
		 * left and the end of the evaluation and returns it. Panics if otherwise.
		 */
		ExprElem evalExpr(query::Context&, const RPNExpr&);
	}

	/**
	 * @brief Query type of the symbol.
	 */
	DECLARE_QUERY(QueryTypeOfSymbol, SymID, ts::TypeInfo)

	/**
	 * @brief Query ts::TypeInfo from a symbol definition (like struct definition).
	 *
	 * Example:
	 * struct T {
	 *	...
	 * }
	 * - Then we can use this query QueryTypeFromDefinition(T).
	 */
	DECLARE_QUERY(QueryTypeFromDefinition, SymID, ts::TypeInfo);

	/**
	 * @brief Struct returned by the `QueryStructSymbolData` query.
	 */
	struct StructSymbolData {
		/**
		 * @brief Name of the struct in the soure code.
		 */
		base::StrId name;
		/**
		 * @brief Struct's declared methods.
		 */
		std::vector<SymID> methods;
		/**
		 * @brief Struct's declared member variables.
		 */
		std::vector<SymID> members;
		/**
		 * @brief Struct's base classes.
		 */
		std::vector<ts::TypeInfo> bases;
	};

	/**
	 * @brief Query all the information about a struct definition.
	 * Panics if the given `SymID` is not a struct.
	 * More information on `StructSymbolData` in it's definition.
	 */
	DECLARE_QUERY(QueryStructSymbolData, SymID, const StructSymbolData&)

	namespace code {
		struct Expr;
	}

	/**
	 * @brief Return Expr tree of HOUT of a expression assigned to a constant.
	 * @note This query is temporary and is used for testing only.
	 */
	DECLARE_QUERY(QueryHOUTExprTreeOfSym, SymID, base::borrow_ptr<const code::Expr>);
}
