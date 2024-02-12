#include <typesystem/typesystem.hpp>
#include "rift_symbols.hpp"
#include "analysis_state.hpp"
#include <base/exceptions.hpp>
#include <base/defer.hpp>

namespace hir {


	base::unique_ptr<Symbol> makeSymbolFromStatement(
		AnalysisState& state, symtable::ScopeRef scope, PstRef<pst::Stmt> stmt
	) {
		switch (stmt->getKind()) {
		// @TODO: cast check
		case pst::StmtKind::Fun: {
			PstRef<pst::Fun> fun = stmt;
			break;
		}
		case pst::StmtKind::Namespace: {
			PstRef<pst::Namespace> namespace_ = stmt;
			return base::make_unique<NamespaceSymbol>(
				state, scope, namespace_->getName(), namespace_
			);
		}
		case pst::StmtKind::Const: {
			PstRef<pst::Const> const_ = stmt;
			return base::make_unique<ConstSymbol>(state, scope, const_->getName(), const_);
		}
		case pst::StmtKind::Struct: {
			PstRef<pst::Struct> struct_ = stmt;
			return base::make_unique<StructSymbol>(state, scope, struct_->getName(), struct_);
		}

		case pst::StmtKind::Alias: {
			PstRef<pst::Alias> alias = stmt;
			return base::make_unique<AliasSymbol>(state, scope, alias->getName(), alias);
		}

		case pst::StmtKind::Using: {
			PstRef<pst::Using> using_ = stmt;
			return base::make_unique<UsingSymbol>(state, scope, base::StrId("wildcard"), using_);
		}

		case pst::StmtKind::EagerLookup: {
			std::cerr << "skipping: EagerLookup \n";
			return nullptr;
		}

		default:
			break;
		}
		RIFT_PANIC(
			base::strConcat("makeSymbolFromStatement bad symbol kind, stmt: ", typeid(*stmt).name())
		);
	}

	void goOverSymbols(
		AnalysisState& state, ScopeRef scope, pst::ParserCBorrowRef<pst::CodeBlock> pst_element
	) {
		// @FUTURE: somewhere here will happen macro expansion
		for (auto& stmt: pst_element->getStatements()) {
			auto sym = makeSymbolFromStatement(state, scope, stmt.borrow());
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
		  Symbol(state, scope, name, false, true, SymbolKind::CompilationUnit),
		  pst_element(pst_element) {}

	void TopLevelSymbol::calculateType() {
		type = ts::TypeDesc<ts::TypeInfo>(ts::ModuleInfo::create());
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
		type = ts::TypeDesc<ts::TypeInfo>(ts::NamespaceInfo::create());
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
		type = ts::TypeDesc<ts::TupleInfo>(ts::MetaInfo::create());
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
		  GenericAlias(state, scope, name, false, true, SymbolKind::Alias),
		  pst_element(pst_element) {
		is_alias = true;
	}

	void AliasSymbol::calculateType() {
		// @TODO
		type = ts::TypeDesc<ts::TypeInfo>(ts::IntegralInfo::create(64));
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

		linked_lookup_scope     = as_single.back()->requestLinkedLookupScope();
		dealiased_lookup_result = std::move(dealiased_single);
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
		  GenericAlias(state, scope, name, false, true, SymbolKind::Alias),
		  pst_element(pst_element) {
		wildcard = true;
		is_alias = true;
	}

	void UsingSymbol::calculateType() {
		// @TODO
		type = ts::TypeDesc<ts::TypeInfo>(ts::IntegralInfo::create(64));
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

		linked_lookup_scope     = as_single.back()->requestLinkedLookupScope();
		dealiased_lookup_result = std::move(dealiased_single);
	}

}
