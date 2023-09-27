#include "rift_symbols.hpp"
#include "analysis_state.hpp"
#include <base/exceptions.hpp>
#include <typesystem/typesystem.hpp>

namespace hir {

	base::unique_ptr<Symbol>
		makeSymbolFromStatement(symtable::ScopeRef scope, PstRef<pst::Stmt> stmt) {
		switch (stmt->getKind()) {
		// @TODO: cast check
		case pst::StmtKind::Fun: {
			PstRef<pst::Fun> fun = stmt;
			std::cerr << "Function: " << fun->getName().strView() << ", skipping\n";
			break;
		}
		case pst::StmtKind::Namespace: {
			PstRef<pst::Namespace> namespace_ = stmt;
			return base::make_unique<NamespaceSymbol>(scope, namespace_->getName(), namespace_);
		}
		case pst::StmtKind::Const: {
			PstRef<pst::Const> const_ = stmt;
			std::cerr << "Const: " << const_->getName().strView() << "\n";
			return base::make_unique<ConstSymbol>(scope, const_->getName(), const_);
		}
		case pst::StmtKind::Struct: {
			PstRef<pst::Struct> struct_ = stmt;
			std::cerr << "Struct: " << struct_->getName().strView() << "\n";
			return base::make_unique<StructSymbol>(scope, struct_->getName(), struct_);
		}

		case pst::StmtKind::Alias: {
			PstRef<pst::Alias> alias = stmt;
			std::cerr << "Alias: " << alias->getName().strView() << "\n";
			return base::make_unique<AliasSymbol>(scope, alias->getName(), alias);
		}

		case pst::StmtKind::Using: {
			PstRef<pst::Using> using_ = stmt;
			std::cerr << "Using: " << using_->getPointed()[0].strView() << "\n";

			return base::make_unique<UsingSymbol>(scope, base::StrId("wildcard"), using_);
		}

		case pst::StmtKind::EagerLookup: {
			std::cerr << "skipping: EagerLookup \n";
			return nullptr;
		}

		default:
			break;
			// RIFT_PANIC("makeSymbolFromStatement bad symbol kind");
		}
		RIFT_PANIC(
			base::strConcat("makeSymbolFromStatement bad symbol kind, stmt: ", typeid(*stmt).name())
		);
	}

	void goOverSymbols(
		AnalysisState &state, ScopeRef scope, pst::ParserCBorrowRef<pst::CodeBlock> pst_element
	) {
		// @TODO: stmts/usings/alias/expand/...
		// using and alias are just symbols
		for (auto &stmt : pst_element->getStatements()) {
			std::cerr << "stmt...\n";
			auto sym = makeSymbolFromStatement(scope, stmt.borrow());
			// @TODO: error symbol
			if (sym != nullptr) state.addSymbol(std::move(sym));
		}
	}

	TopLevelSymbol::TopLevelSymbol(
		ScopeRef scope, base::StrId name, PstRef<pst::TopLevel> pst_element
	):
		  Symbol(scope, name, false, true, SymbolKind::CompilationUnit),
		  pst_element(pst_element) {}

	void TopLevelSymbol::calculateType() {
		type = ts::TypeDesc<ts::TypeInfo>(ts::ModuleInfo::create());
	}

	void TopLevelSymbol::analyzeAll(AnalysisState &state) {
		getAll(state);

		// @TODO: stmts/usings/alias/expand/...
		// using and alias are just symbols
		for (auto &stmt : pst_element->getStatements()) {
			std::cerr << "stmt...\n";
			state.addSymbol(makeSymbolFromStatement(scope, stmt.borrow()));
		}
	}

	void NamespaceSymbol::calculateType() {
		type = ts::TypeDesc<ts::TypeInfo>(ts::NamespaceInfo::create());
	}

	void NamespaceSymbol::analyzeAll(AnalysisState &state) {
		getAll(state);

		auto inner_scope = getLinkedLookupScope(state);
		goOverSymbols(state, inner_scope, pst_element->getBody());
	}

	void ConstSymbol::calculateType() {
		// @TODO look up here and other stuff
		type = ts::TypeDesc<ts::TypeInfo>(ts::IntegralInfo::create(64));
	}

	void ConstSymbol::analyzeAll(AnalysisState &state) { getAll(state); }

	ts::ClassInfo StructSymbol::calculateValue() {
		//...
		// @TODO
		return ts::ClassInfo::create(base::StrId("A"), {});
	}

	void StructSymbol::calculateType() {
		type = ts::TypeDesc<ts::TupleInfo>(ts::MetaInfo::create());
	}

	void StructSymbol::analyzeAll(AnalysisState &) {
		// @TODO
	}

	symtable::SymbolChain GenericAlias::getUniqueDeAlias(hir::AnalysisState &state) {
		if (!dealiased_lookup_result.has_value()) {
			// this can be confusing:
			calculateLinkedLookup(state);
		}
		return dealiased_lookup_result.value();
	}

	symtable::ChainLookupResult GenericAlias::getDeAlias(hir::AnalysisState &state) {
		throw base::NotYetImplemented("getDeAlias -- only require dealiasing any lookup results");
	}

	AliasSymbol::AliasSymbol(ScopeRef scope, base::StrId name, PstRef<pst::Alias> pst_element):
		  GenericAlias(scope, name, false, true, SymbolKind::Const),
		  pst_element(pst_element) {
		is_alias = true;
	}

	void AliasSymbol::calculateType() {
		// @TODO
		type = ts::TypeDesc<ts::TypeInfo>(ts::IntegralInfo::create(64));
	}

	void AliasSymbol::calculateLinkedLookup(AnalysisState &state) {
		std::cerr << "  AliasSymbol -- calculating linked lookup"
				  << "\n";

		auto names         = pst_element->getPointed();
		auto lookup_result = state.symTable().lookupDottedNameInScopeAndParents(
			state, scope, { names.begin(), names.end() }
		);

		std::cerr << "Got lookup result:\n";
		lookup_result.dprint(std::cerr);
		std::cerr << "\n";

		if (!lookup_result.isSingle()) RIFT_PANIC("ambiguity in alias, @TODO: error in state");

		auto as_single        = lookup_result.getAsSingle();
		auto dealiased_single = symtable::deAliasSymbolChain(state, as_single);

		// @TODO: in future alias should not necessary be single
		// alias to "overloaded"

		symtable::dprintSymbolChain(as_single, std::cerr);
		std::cerr << "\n";
		std::cerr << "deAliased: ";
		symtable::dprintSymbolChain(symtable::deAliasSymbolChain(state, as_single), std::cerr);
		std::cerr << "\n";

		linked_lookup_scope     = as_single.back()->getLinkedLookupScope(state);
		dealiased_lookup_result = std::move(dealiased_single);
		// @TODO: some ok here?
		// Or just ErrorSymbol propagation
	}

	void AliasSymbol::analyzeAll(AnalysisState &state) {
		// @TODO
		getAll(state);
	}

	UsingSymbol::UsingSymbol(ScopeRef scope, base::StrId name, PstRef<pst::Using> pst_element):
		  GenericAlias(scope, name, false, true, SymbolKind::Const),
		  pst_element(pst_element) {
		wildcard = true;
		is_alias = true;
	}

	void UsingSymbol::calculateType() {
		// @TODO
		type = ts::TypeDesc<ts::TypeInfo>(ts::IntegralInfo::create(64));
	}

	void UsingSymbol::analyzeAll(AnalysisState &state) {
		// @TODO
		getAll(state);
	}

	void UsingSymbol::calculateLinkedLookup(AnalysisState &state) {
		std::cerr << "  UsingSymbol -- calculating linked lookup"
				  << "\n";

		auto names         = pst_element->getPointed();
		auto lookup_result = state.symTable().lookupDottedNameInScopeAndParents(
			state, scope, { names.begin(), names.end() }
		);

		std::cerr << "Got lookup result:\n";
		lookup_result.dprint(std::cerr);
		std::cerr << "\n";

		if (!lookup_result.isSingle()) RIFT_PANIC("ambiguity in using, @TODO: error in state");
		auto as_single        = lookup_result.getAsSingle();
		auto dealiased_single = symtable::deAliasSymbolChain(state, as_single);

		// @TODO: in future alias should not necessary be single
		// using to "overloaded"

		symtable::dprintSymbolChain(as_single, std::cerr);
		std::cerr << "\n";
		std::cerr << "deAliased: ";
		symtable::dprintSymbolChain(dealiased_single, std::cerr);
		std::cerr << "\n";

		linked_lookup_scope     = as_single.back()->getLinkedLookupScope(state);
		dealiased_lookup_result = std::move(dealiased_single);
	}

	// TestEagerLookupSymbol::TestEagerLookupSymbol(
	// 	ScopeId scope, base::StrId name,
	// 	PstRef<pst::Const> pst_element):
	// 		Symbol(scope, name, false, true, SymbolKind::TestSymbol),
	// 		pst_element(pst_element) {

	// 	state.
	// }

	// void TestEagerLookupSymbol::calculateType() {
	// 	// placeholder type:
	// 	type = ts::TypeDesc<ts::TupleInfo>(0, ts::VoidInfo::create());
	// }

	// void TestEagerLookupSymbol::analyzeAll(AnalysisState&) {
	// 	getKind();
	// 	getType();
	// }
}
