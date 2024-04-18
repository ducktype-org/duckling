#include "symbols.hpp"
#include "base/perfect_hash.hpp"
#include "pst_parser/elements/elements.hpp"
#include "../pst_ref.hpp"
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
		bool        anonymous = false;
		bool        wildcard = false;
		bool        is_alias = false;
		bool        dependent = false;
		SymbolKind  kind;
		// pst link? -- what about down casting...

		// we will need to cast it:
		PstRef<pst::Stmt> pst_stmt;


		// cached: linked_lookup_scope ?
		// cached: type
		// cached: value?
	};

	// do we want internal inheritance?
	// query: lookupIn
	// query: dealias

	struct GetSymRef_Functor {
		static auto get(SymID id) { return id.ref; }
	};
	auto getSymRef(SymID id) { return GetSymRef_Functor::get(id); }
	

	base::StrId name(SymID id) {
		return getSymRef(id)->name;
	}
	SymbolKind kind(SymID id) {
		return getSymRef(id)->kind;
	}
	ScopeID scope(SymID id) {
		return getSymRef(id)->scope;
	}


	namespace {
		// global table:
		base::StableVector<SymbolData> symbol_table;

		template<class... T>
		auto putInSymtable(T&&... args) {
			auto key = symbol_table.emplaceBack(std::forward<T>(args)...);
			return symbol_table.getRef(key).value();
		}
	}


	// Symbol Factory:
	base::borrow_ptr<SymbolData> makeSymbolFromStatement(
		ScopeID scope, PstRef<pst::Stmt> stmt
	) {
		switch (stmt->getKind()) {
		case pst::StmtKind::Fun: {
			break;
		}
		case pst::StmtKind::Namespace: {
			auto namespace_ = dynamic_cast<const pst::Namespace*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = namespace_->getName(),
				.kind     = SymbolKind::Namespace,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Const: {
			auto const_ = dynamic_cast<const pst::Const*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = const_->getName(),
				.kind     = SymbolKind::Namespace,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Struct: {
			break;
		}
		case pst::StmtKind::Alias: {
			break;
		}
		case pst::StmtKind::Using: {
			break;
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
		

		static auto provide(Context&, QKey key) -> PResult {
			return PResult{ makeSymbolFromStatement(key.scope, key.stmt) };
		}

		static auto load([[maybe_unused]] QKey key) -> LoadResult { return {}; }

		static auto store([[maybe_unused]] QKey key, PResult res, [[maybe_unused]] query::ACD acd) -> QResult {
			throw "TODO";
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QuerySymbolOfSTMT, "Query Symbol of Stmt");

	// if somewhere then here it is needed to handle cycles somehow

	base::HashT KeyOf_QuerySymbolOfSTMT::customPerfectHash() const {
		auto hash_1 = base::perfectHash(scope);
		auto hash_2 = stmt->getID().asInt();

		// @FIXME: this does not work:
		return hash_1*143 + hash_2*7;
	}

}