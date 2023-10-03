#pragma once

#include "symtable/symbol.hpp"
#include "symtable/symbol_ref.hpp"
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/rift_parser_base.hpp>

#include <typesystem/typesystem.hpp>

#include <optional>

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

	class TopLevelSymbol: public symtable::Symbol {
		// symbol representing single file/compilation unit
		PstRef<pst::TopLevel> pst_element;

	public:
		TopLevelSymbol(ScopeRef scope, base::StrId name, PstRef<pst::TopLevel> pst_element);

		void calculateType() override;
		void analyzeAll(AnalysisState&) override;
	};

	class NamespaceSymbol: public symtable::Symbol {
		PstRef<pst::Namespace> pst_element;

	public:
		NamespaceSymbol(ScopeRef scope, base::StrId name, PstRef<pst::Namespace> pst_element):
			  Symbol(scope, name, false, true, SymbolKind::Namespace),
			  pst_element(pst_element) {}

		void calculateType() override;
		void analyzeAll(AnalysisState&) override;
	};

	// @TODO: StructSymbol should technically be isomorphic with `const a: type =
	// magic_struct_value`. Perhaps merge them in the future
	class StructSymbol: public symtable::Symbol {
		PstRef<pst::Struct>          pst_element;
		std::optional<ts::ClassInfo> value;

	public:
		StructSymbol(ScopeRef scope, base::StrId name, PstRef<pst::Struct> pst_element):
			  Symbol(scope, name, false, true, SymbolKind::Struct),
			  pst_element(pst_element) {}

		ts::ClassInfo calculateValue();
		void          calculateType() override;
		void          analyzeAll(AnalysisState&) override;
	};

	class GlobalVarSymbol: public symtable::Symbol {
		// value?
	};

	class ConstSymbol: public symtable::Symbol {
		// optional calculated Value
		PstRef<pst::Const> pst_element;

	public:
		ConstSymbol(ScopeRef scope, base::StrId name, PstRef<pst::Const> pst_element):
			  Symbol(scope, name, false, true, SymbolKind::Const),
			  pst_element(pst_element) {}

		void calculateType() override;
		void analyzeAll(AnalysisState&) override;
	};

	class GenericAlias: public symtable::Symbol {
	protected:
		// @TODO: this should be ChainLookupResult
		// it is SymbolChain for now, because only SymbolChain can be dealiased
		option<symtable::SymbolChain> dealiased_lookup_result;

	public:
		using symtable::Symbol::Symbol;

		symtable::SymbolChain       getUniqueDeAlias(hir::AnalysisState&) override;
		symtable::ChainLookupResult getDeAlias(hir::AnalysisState&) override;
	};

	class AliasSymbol: public GenericAlias {
		PstRef<pst::Alias> pst_element;

	public:
		AliasSymbol(ScopeRef scope, base::StrId name, PstRef<pst::Alias> pst_element);

		void calculateType() override;
		void analyzeAll(AnalysisState&) override;
		void calculateLinkedLookup(hir::AnalysisState&) override;
	};

	class UsingSymbol: public GenericAlias {
		PstRef<pst::Using> pst_element;

	public:
		UsingSymbol(ScopeRef scope, base::StrId name, PstRef<pst::Using> pst_element);

		void calculateType() override;
		void analyzeAll(AnalysisState&) override;
		void calculateLinkedLookup(hir::AnalysisState&) override;
	};

	class FunSymbol: public symtable::Symbol {
		// TypeInfo
		// some more params/args?
		// inner code
	};

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
	// 	void analyzeAll(AnalysisState&) override;
	// };
}
