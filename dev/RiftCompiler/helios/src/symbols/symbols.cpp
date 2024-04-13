#include "symbols.hpp"
#include "pst_parser/elements/elements.hpp"
#include <memory>
#include <query_framework/query_impl.hpp>
#include <base/stable_container.hpp>

namespace compiler::helios {

	// scopes will look more or the less the same!
	// imports are just symbols that will require lookup inside different module that will build itself from different PST. Simple!
	// We will just need query for import lookup cached by (globally) unique PstID


	struct SymbolData {
		// adapted from hir:

		// created on startup:
		ScopeID     scope;
		base::StrId name;
		bool        anonymous;
		bool wildcard = false;
		bool is_alias = false;
		bool dependent = false;
		SymbolKind kind;
		// pst link? -- what about down casting...


		// cached: linked_lookup_scope ?
		// cached: type
		// cached: value?
		



	};

	// do we want internal inheritance?
	// query: lookupIn
	// query: dealias
	

	namespace {
		// global table:
		base::StableVector<SymbolData> symbol_table;
	}


	// Symbol Factory:
	base::borrow_ptr<SymbolData> makeSymbolFromStatement(
		ScopeID scope, PstRef<pst::Stmt> stmt
	) {
		PstRef<pst::Fun> fun = stmt;
		const pst::Fun* f = static_cast<const pst::Fun*>(stmt.get());

		std::unique_ptr<int> a;

		switch (stmt->getKind()) {
		// @TODO: cast check
		case pst::StmtKind::Fun: {
			// PstRef<pst::Fun> fun = stmt;
			break;
		}
		case pst::StmtKind::Namespace: {
			// PstRef<pst::Namespace> namespace_ = stmt;
			// return base::make_unique<NamespaceSymbol>(
			// 	state, scope, namespace_->getName(), namespace_
			// );
		}
		case pst::StmtKind::Const: {
			// PstRef<pst::Const> const_ = stmt;
			// return base::make_unique<ConstSymbol>(state, scope, const_->getName(), const_);
		}
		case pst::StmtKind::Struct: {
			// PstRef<pst::Struct> struct_ = stmt;
			// return base::make_unique<StructSymbol>(state, scope, struct_->getName(), struct_);
		}

		case pst::StmtKind::Alias: {
			// PstRef<pst::Alias> alias = stmt;
			// return base::make_unique<AliasSymbol>(state, scope, alias->getName(), alias);
		}

		case pst::StmtKind::Using: {
			// PstRef<pst::Using> using_ = stmt;
			// return base::make_unique<UsingSymbol>(state, scope, base::StrId("wildcard"), using_);
		}

		default:
			break;
		}
		RIFT_PANIC(
			base::strConcat("makeSymbolFromStatement bad symbol kind, stmt: ", typeid(stmt).name())
		);
	}



	struct ImplementationOf_QuerySymbolOfSTMT:
		public query::QueryImplementation<QuerySymbolOfSTMT, SymID>
	{
		static auto provide(Context& ctx, QKey key) -> PResult {
			// generate new symbol
			throw "TODO";
		}

		static auto load([[maybe_unused]] QKey key) -> LoadResult { return {}; }

		static auto store([[maybe_unused]] QKey key, PResult res, [[maybe_unused]] query::ACD acd) {
			throw "TODO";
		}
	};

	// if somewhere then here it is needed to handle cycles somehow

}