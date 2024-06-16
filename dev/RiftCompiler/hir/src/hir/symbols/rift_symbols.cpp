#include <typesystem/typesystem.hpp>
#include "rift_symbols.hpp"
#include <hir/analysis_state.hpp>
#include <base/exceptions.hpp>
#include <base/defer.hpp>
#include <utility>

namespace hir {


	base::unique_ptr<Symbol> makeSymbolFromStatement(
		AnalysisState& state, symtable::ScopeRef scope, PstRef<pst::Stmt> stmt
	) {
		switch (stmt->getKind()) {
		// @TODO: cast check
		case pst::StmtKind::Fun: {
			// PstRef<pst::Fun> fun = stmt;
			break;
		}
		case pst::StmtKind::Namespace: {
			auto namespace_
				= PstRef<pst::Namespace>(dynamic_cast<const pst::Namespace*>(stmt.get()));
			return base::make_unique<NamespaceSymbol>(
				state, scope, namespace_->getName(), namespace_
			);
		}
		case pst::StmtKind::Const: {
			auto const_ = PstRef<pst::Const>(dynamic_cast<const pst::Const*>(stmt.get()));
			return base::make_unique<ConstSymbol>(state, scope, const_->getName(), const_);
		}
		case pst::StmtKind::Struct: {
			auto struct_ = PstRef<pst::Struct>(dynamic_cast<const pst::Struct*>(stmt.get()));
			return base::make_unique<StructSymbol>(state, scope, struct_->getName(), struct_);
		}

		case pst::StmtKind::Alias: {
			auto alias = PstRef<pst::Alias>(dynamic_cast<const pst::Alias*>(stmt.get()));
			return base::make_unique<AliasSymbol>(state, scope, alias->getName(), alias);
		}

		case pst::StmtKind::Using: {
			auto using_ = PstRef<pst::Using>(dynamic_cast<const pst::Using*>(stmt.get()));
			return base::make_unique<UsingSymbol>(state, scope, base::StrId("wildcard"), using_);
		}
		case pst::StmtKind::Variable: {
			auto variable_ = PstRef(dynamic_cast<const pst::Variable*>(stmt.get()));
			return base::make_unique<UsingSymbol>(state, scope, base::StrId("variable"), variable_);
		}

		default:
			break;
		}
		RIFT_PANIC(
			base::strConcat("makeSymbolFromStatement bad symbol kind, stmt: ", typeid(stmt).name())
		);
	}

	void goOverSymbols(
		AnalysisState&                               state,
		ScopeRef                                     scope,
		const pst::ParserCBorrowRef<pst::CodeBlock>& pst_element
	) {
		// @FUTURE: somewhere here will happen macro expansion
		for (auto stmt: pst_element) {
			auto sym = makeSymbolFromStatement(state, scope, stmt);
			// @TODO: error symbol
			if (sym != nullptr) state.addSymbol(std::move(sym));
		}
	}

	TopLevelSymbol::TopLevelSymbol(
		hir::AnalysisState&   state,
		ScopeRef              scope,
		base::StrId           name,
		PstRef<pst::TopLevel> pst_element
	):
		  Symbol(state, std::move(scope), name, false, true, SymbolKind::CompilationUnit),
		  pst_element(std::move(pst_element)) {}

	void TopLevelSymbol::calculateType() {
		type = ts::TypeDesc(query::entryPoint<ts::QueryModuleType>({}));
	}

	void TopLevelSymbol::analyzeAll() { getAll(); }

	void TopLevelSymbol::getSymbolsIn() {
		if (symbol_in_done) return;
		symbol_in_done = true;

		auto inner_scope = requestLinkedLookupScope();

		// @TODO: stmts/usings/alias/expand/...
		// using and alias are just symbols
		for (auto& stmt: pst_element->getStatements()) {
			analysis_state.addSymbol(
				makeSymbolFromStatement(analysis_state, inner_scope, stmt.borrow())
			);
		}
	}

	void NamespaceSymbol::calculateType() {
		type = ts::TypeDesc(query::entryPoint<ts::QueryNamespaceType>({}));
	}

	void NamespaceSymbol::analyzeAll() { getAll(); }

	void NamespaceSymbol::getSymbolsIn() {
		if (symbol_in_done) return;
		symbol_in_done = true;

		auto inner_scope = requestLinkedLookupScope();
		goOverSymbols(analysis_state, inner_scope, pst_element->getBody());
	}

	void ConstSymbol::calculateType() { type = type_expr->evalAsType(analysis_state); }

	void ConstSymbol::analyzeAll() { getAll(); }

	exec::CTV ConstSymbol::requestValue() {
		if (value)
			return *value;
		else
			value = value_expr->eval(analysis_state);
		return value_expr->eval(analysis_state);
	}

	ts::ClassInfo StructSymbol::calculateValue() {
		// @TODO
		return ts::ClassInfo::create(base::StrId("A"), {});
	}

	void StructSymbol::calculateType() {
		// TODO
	}

	void StructSymbol::analyzeAll() {
		// @TODO
	}

	void StructSymbol::getSymbolsIn() {
		if (symbol_in_done) return;
		symbol_in_done = true;

		// @TODO
	}

	symtable::SymbolChain GenericAlias::requestUniqueDeAlias() {
		std::cerr << "  > requestUniqueDeAlias of " << getName().strView() << "\n";
		if (!dealiased_lookup_result.has_value()) {
			std::cerr << "  > calculating...\n";
			// this can be confusing:
			// we need it to ensure that dealiased_lookup_result has value
			calculateLinkedLookup();
		}
		return dealiased_lookup_result.value();
	}

	symtable::ChainLookupResult GenericAlias::requestDeAlias() {
		throw base::NotYetImplemented("requestDeAlias -- only require dealiasing any lookup results"
		);
	}

	AliasSymbol::AliasSymbol(
		hir::AnalysisState& state, ScopeRef scope, base::StrId name, PstRef<pst::Alias> pst_element
	):
		  GenericAlias(state, std::move(scope), name, false, true, SymbolKind::Alias),
		  pst_element(std::move(pst_element)) {
		is_alias = true;
	}

	void AliasSymbol::calculateType() {
		// @TODO
		type = query::entryPoint<ts::QueryIntegralType>({ 64 });
	}

	void AliasSymbol::calculateLinkedLookup() {
		std::cerr << "  AliasSymbol -- calculating linked lookup"
				  << "\n";

		auto names         = pst_element->getPointed();
		auto lookup_result = analysis_state.symTable().lookupDottedNameInScopeAndParents(
			scope, { names.begin(), names.end() }
		);

		std::cerr << "Got lookup result:\n";
		lookup_result.dprint(std::cerr);
		std::cerr << "\n";

		if (!lookup_result.isSingle()) RIFT_PANIC("ambiguity in alias, @TODO: error in state");

		auto as_single        = lookup_result.getAsSingle();
		auto dealiased_single = symtable::deAliasSymbolChain(as_single);

		// @TODO: in future alias should not necessary be single
		// alias to "overloaded"

		symtable::dprintSymbolChain(as_single, std::cerr);
		std::cerr << "\n";
		std::cerr << "deAliased: ";
		symtable::dprintSymbolChain(symtable::deAliasSymbolChain(as_single), std::cerr);
		std::cerr << "\n";

		linked_lookup_scope        = as_single.back()->requestLinkedLookupScope();
		getDealiasedLookupResult() = std::move(dealiased_single);
		// @TODO: some ok here?
		// Or just ErrorSymbol propagation
	}

	void AliasSymbol::analyzeAll() {
		// @TODO
		getAll();
	}

	UsingSymbol::UsingSymbol(
		hir::AnalysisState& state, ScopeRef scope, base::StrId name, PstRef<pst::Using> pst_element
	):
		  GenericAlias(state, std::move(scope), name, false, true, SymbolKind::Alias),
		  pst_element(std::move(std::move(pst_element))) {
		wildcard = true;
		is_alias = true;
	}

	void UsingSymbol::calculateType() {
		// @TODO
		type = query::entryPoint<ts::QueryIntegralType>({ 64 });
	}

	void UsingSymbol::analyzeAll() {
		// @TODO
		getAll();
	}

	void UsingSymbol::calculateLinkedLookup() {
		std::cerr << "  UsingSymbol -- calculating linked lookup"
				  << "\n";
		RIFT_ASSERT(unlockedLookup(), "calculateLinkedLookup in locked lookup!");
		lock_lookup = true;
		defer(lock_lookup = false);


		auto names         = pst_element->getPointed();
		auto lookup_result = analysis_state.symTable().lookupDottedNameInScopeAndParents(
			scope, { names.begin(), names.end() }
		);

		std::cerr << "Got lookup result:\n";
		lookup_result.dprint(std::cerr);
		std::cerr << "\n";


		if (!lookup_result.isSingle()) RIFT_PANIC("ambiguity in using, @TODO: error in state");
		auto as_single        = lookup_result.getAsSingle();
		auto dealiased_single = symtable::deAliasSymbolChain(as_single);

		// @TODO: in future alias should not necessary be single
		// using to "overloaded"

		symtable::dprintSymbolChain(as_single, std::cerr);
		std::cerr << "\n";
		std::cerr << "deAliased: ";
		symtable::dprintSymbolChain(dealiased_single, std::cerr);
		std::cerr << "\n";

		linked_lookup_scope        = as_single.back()->requestLinkedLookupScope();
		getDealiasedLookupResult() = std::move(dealiased_single);
	}

}
