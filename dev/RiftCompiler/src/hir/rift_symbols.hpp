/**
 * @file rift_symbols.hpp
 * @brief This file holds class definitions of all HIR-symbols.
 */

#pragma once

#include "symtable/symbol.hpp"
#include "symtable/symbol_ref.hpp"
#include "hir_expr.hpp"
#include <pst_parser/rift_parser_base.hpp>
#include <pst_parser/elements/elements.hpp>

#include <typesystem/typesystem.hpp>

#include <base/optional.hpp>

namespace hir {

	using symtable::ScopeRef;
	using symtable::SymbolKind;
	using symtable::SymbolRef;

	template<typename Element>
	using PstRef = pst::ParserCBorrowRef<Element>;

	enum class CycleState {
		UnVisited,
		InProgress,
		Done,
	};

	/**
	 * @brief Represent a "File"
	 */
	class TopLevelSymbol: public symtable::Symbol {
		// symbol representing single file/compilation unit
		PstRef<pst::TopLevel> pst_element;

	public:
		TopLevelSymbol(
			hir::AnalysisState&   state,
			ScopeRef              scope,
			base::StrId           name,
			PstRef<pst::TopLevel> pst_element
		);

		void         calculateType() override;
		void         analyzeAll() override;
		virtual void getSymbolsIn() override;
	};

	/**
	 * @brief Represent a Namespace declaration
	 */
	class NamespaceSymbol: public symtable::Symbol {
		PstRef<pst::Namespace> pst_element;

	public:
		NamespaceSymbol(
			hir::AnalysisState&    state,
			ScopeRef               scope,
			base::StrId            name,
			PstRef<pst::Namespace> pst_element
		):
			  Symbol(state, scope, name, false, true, SymbolKind::Namespace),
			  pst_element(pst_element) {}

		void         calculateType() override;
		void         analyzeAll() override;
		virtual void getSymbolsIn() override;
	};

	/**
	 * @brief Represent a Structure declaration.
	 * @TODO: StructSymbol should technically be isomorphic with `const a: type = magic_struct_value`. Perhaps merge them in the future
	 */
	class StructSymbol: public symtable::Symbol {
		PstRef<pst::Struct>           pst_element;
		base::Optional<ts::ClassInfo> value;

	public:
		StructSymbol(
			hir::AnalysisState& state,
			ScopeRef            scope,
			base::StrId         name,
			PstRef<pst::Struct> pst_element
		):
			  Symbol(state, scope, name, false, true, SymbolKind::Struct),
			  pst_element(pst_element) {}

		void         calculateType() override;
		void         analyzeAll() override;
		virtual void getSymbolsIn() override;

		// @deprecated
		ts::ClassInfo calculateValue();
	};

	/**
	 * @brief Represents a global variable declaration.
	 */
	class GlobalVarSymbol: public symtable::Symbol {
		// value?
	};

	/**
	 * @brief Represents a constant declaration.
	 */
	class ConstSymbol: public symtable::Symbol {
		// optional calculated Value
		PstRef<pst::Const> pst_element;

		ExpressionRef type_expr;
		ExpressionRef value_expr;

		base::Optional<exec::CTV> value;

	public:
		ConstSymbol(
			hir::AnalysisState& state,
			ScopeRef            scope,
			base::StrId         name,
			PstRef<pst::Const>  pst_element
		):
			  Symbol(state, scope, name, false, true, SymbolKind::Const),
			  pst_element(pst_element),
			  type_expr(Expression::makeExpr(scope, pst_element->getType())),
			  value_expr(Expression::makeExpr(scope, pst_element->getValue())) {}

		void calculateType() override;
		void analyzeAll() override;

		exec::CTV requestValue() final;
	};

	/**
	 * @brief Super class for aliases.
	 */
	class GenericAlias: public symtable::Symbol {
	protected:
		// @TODO: this should be ChainLookupResult
		// it is SymbolChain for now, because only SymbolChain can be dealiased
		base::Optional<symtable::SymbolChain> dealiased_lookup_result;

	public:
		using symtable::Symbol::Symbol;

		symtable::SymbolChain       requestUniqueDeAlias() override;
		symtable::ChainLookupResult requestDeAlias() override;
	};

	/**
	 * @brief Represents alias of form `alias x = y`
	 */
	class AliasSymbol: public GenericAlias {
		PstRef<pst::Alias> pst_element;

	public:
		AliasSymbol(
			hir::AnalysisState& state,
			ScopeRef            scope,
			base::StrId         name,
			PstRef<pst::Alias>  pst_element
		);

		void calculateType() override;
		void analyzeAll() override;
		void calculateLinkedLookup() override;
	};

	/**
	 * @brief Represents alias of form `using y.*`
	 */
	class UsingSymbol: public GenericAlias {
		PstRef<pst::Using> pst_element;

	public:
		UsingSymbol(
			hir::AnalysisState& state,
			ScopeRef            scope,
			base::StrId         name,
			PstRef<pst::Using>  pst_element
		);

		void calculateType() override;
		void analyzeAll() override;
		void calculateLinkedLookup() override;
	};
	
	/**
	 * @brief Represents a function declaration.
	 */
	class FunSymbol: public symtable::Symbol {
		// TypeInfo
		// some more params/args?
		// inner code
	};

	/**
	 * @brief Represents an local variole declaration 
	 */
	class VarSymbol: public symtable::Symbol {
		// TypeInfo
		// value will be elsewhere
	};

	/**
	 * @brief Tests lookup made before scan of symbols inside
	 * the scope.
	 * Used by: early using statement
	 */
	// class TestEagerLookupSymbol: public symtable::Symbol {
	// 	PstRef<pst::EagerLookup> pst_element;
	// public:
	// 	TestEagerLookupSymbol(ScopeId scope, base::StrId name,
	// 	                      PstRef<pst::Const> pst_element);

	// 	void calculateType() override;
	// 	void analyzeAll() override;
	// };
}
