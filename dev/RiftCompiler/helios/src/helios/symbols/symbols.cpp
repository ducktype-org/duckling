#include "symbols.hpp"
#include "base/exceptions.hpp"
#include "base/perfect_hash.hpp"
#include "base/raw_view.hpp"
#include "base/string_id.hpp"
#include "helios/lookup_result.hpp"
#include "helios/scope_symbol_id.hpp"
#include "helios/scopes/scopes.hpp"
#include "pst_parser/elements/elements.hpp"
#include "../pst_ref.hpp"
#include <query_framework/query_impl.hpp>
#include <base/stable_container.hpp>
#include <base/stable_hashmap.hpp>
#include <vector>

namespace compiler::helios {

	/**
	 * @TODO: move to some docs
	 *  * imports are just symbols that we will "lookup in" just like usings.
	 *    They will link to different modules.
	 *  * Scopes trees of different modules are independent to relax dependency
	 *  
	 *  @TODO: what about lookup cycles -- we will need to probably refactor queries a bit
	 *  in the future
	 */


	/**
	 * @brief Stores generic symbol data
	 */
	struct SymbolData {
		// created when creating SymbolData:
		ScopeID     scope;
		base::StrId name;
		bool        anonymous   = false;
		bool        is_wildcard = false;
		bool        is_alias    = false;
		bool        dependent   = false;
		SymbolKind  kind;

		// we will need to cast it:
		PstRef<pst::Stmt> pst_stmt;
	};

	/**
	 * @brief Helper struct used to access private SymID data.
	 */
	struct GetSymRef_Functor {
		static auto get(SymID id) { return id.ref; }
	};

	auto getSymRef(SymID id) { return GetSymRef_Functor::get(id); }

	bool isWildcard(SymID id) { return getSymRef(id)->is_wildcard; }

	base::StrId name(SymID id) { return getSymRef(id)->name; }

	SymbolKind kind(SymID id) { return getSymRef(id)->kind; }

	ScopeID scope(SymID id) { return getSymRef(id)->scope; }

	namespace {
		/**
		 * @brief Global Symbol Table
		 * @note: in the future it might not be needed once
		 * we will move toward more pure Query Model
		 */
		base::StableVector<SymbolData> symbol_table;

		template<class... T>
		auto putInSymtable(T&&... args) {
			auto key = symbol_table.emplaceBack(std::forward<T>(args)...);
			return symbol_table.getRef(key).value();
		}
	}

	/**
	 * @brief SymbolData Factory
	 * 
	 * @param scope 
	 * @param stmt 
	 * @return base::borrow_ptr<SymbolData> 
	 */
	base::borrow_ptr<SymbolData>
		makeSymbolFromStatement(const ScopeID& scope, PstRef<pst::Stmt> stmt) {
		switch (stmt->getKind()) {
		case pst::StmtKind::Fun: {
			auto&& function_ = dynamic_cast<const pst::Fun*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = function_->getName(),
				.kind     = SymbolKind::Function,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Namespace: {
			auto&& namespace_ = dynamic_cast<const pst::Namespace*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = namespace_->getName(),
				.kind     = SymbolKind::Namespace,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Const: {
			auto&& const_ = dynamic_cast<const pst::Const*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = const_->getName(),
				.kind     = SymbolKind::Const,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Struct: {
			auto&& struct_ = dynamic_cast<const pst::Struct*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = struct_->getName(),
				.kind     = SymbolKind::Struct,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Alias: {
			auto&& alias_ = dynamic_cast<const pst::Alias*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = alias_->getName(),
				.is_alias = true,
				.kind     = SymbolKind::Alias,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Using: {
			auto&& using_ = dynamic_cast<const pst::Using*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope = scope,
				.name
				= base::StrId(base::strConcat("<USING> ", using_->getPointed().front()).c_str()),
				.is_wildcard = true,
				.is_alias    = true,
				.kind        = SymbolKind::Using,
				.pst_stmt    = stmt,
			});
		}

		default:
			break;
		}
		RIFT_PANIC(
			base::strConcat("makeSymbolFromStatement bad symbol kind, stmt: ", typeid(stmt).name())
		);
	}

	struct ImplementationOf_QuerySymbolOfSTMT:
		  query::QueryImplementation<QuerySymbolOfSTMT, SymID> {
		static auto provide(Context&, QKey key) -> PResult {
			return PResult{ makeSymbolFromStatement(key.scope, key.stmt) };
		}

		// @OPT: opt it?
		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QuerySymbolOfSTMT, "Query Symbol of Stmt");


	struct ImplementationOf_QueryLookupInSymbol:
		  public query::QueryImplementation<QueryLookupInSymbol, LookupResult> {
		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.symbol.ref->kind) {
			case SymbolKind::Using:
			case SymbolKind::Namespace: {
				auto linked_scope = ctx.query<QueryLinkedScope>(key.symbol);
				return ctx.query<QueryLookupInScope>(
					{ linked_scope, key.name, key.follow_wildcards }
				);
			}

			// @note: here case for variables will be calling TS
			default:
				throw base::NotYetImplemented("Lookup in symbol...");
			}
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryLookupInSymbol, "QueryLookupInSymbol")


	// @FIXME: make this legit
	SymbolList lookupChain(
		query::detail::ContextType& ctx,
		std::vector<base::StrId>    names,
		ScopeID                     begin_scope,
		bool                        follow_wildcards
	) {
		RIFT_ASSERT(names.size() > 0, "lookupDotted received zero names");

		// initial symbol:
		auto first
			= ctx.query<QueryLookupInScopeAndParents>({ begin_scope, names[0], follow_wildcards });

		if (not first.isSingle()) {
			// @TODO: error in state
			RIFT_PANIC("ambiguity in lookupChain");
		}

		if (names.size() == 1) return first.getAsSingle();

		SymbolList result = first.getAsSingle();

		for (usize i = 1; i < names.size(); i++) {
			auto append_res
				= ctx.query<QueryLookupInSymbol>({ result.back(), names[i], follow_wildcards });

			if (!append_res.isSingle()) {
				// @TODO: error in state
				RIFT_PANIC("ambiguity in lookup");
			}

			auto single_append_res = append_res.getAsSingle();
			result.insert(result.end(), single_append_res.begin(), single_append_res.end());
		}
		return result;
	}

	struct ImplementationOf_QueryLinkedScope:
		  public query::QueryImplementation<QueryLinkedScope, ScopeID> {
		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.ref->kind) {
			case SymbolKind::Using: {
				auto using_stmt = dynamic_cast<const pst::Using*>(key.ref->pst_stmt.get());
				auto names      = using_stmt->getPointed();
				auto lookup_res = lookupChain(ctx, names, scope(key), false);
				RIFT_ASSERT(
					not lookup_res.empty(), "Well, i honestly don't know what that means, good luck"
				);
				return ctx.query<QueryLinkedScope>({ lookup_res.back() });
			}
			case SymbolKind::Namespace: {
				auto namespace_stmt = dynamic_cast<const pst::Namespace*>(key.ref->pst_stmt.get());
				auto inner_scope    = ctx.query<QueryPrimaryCodeScopeFor>({
                    scope(key),
                    namespace_stmt->getBody(),
                });
				return inner_scope;
			}
			default:
				throw base::NotYetImplemented("Getting linked scope...");
			}
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryLinkedScope, "QueryLookupInSymbol")

	base::HashT KeyOf_QuerySymbolOfSTMT::customPerfectHash() const {
		auto hash_1 = base::perfectHash(scope);
		auto hash_2 = stmt->getID().asInt();

		// @FIXME: this does not work:
		return hash_1 * 143 + hash_2 * 7;
	}

	base::HashT KeyOf_LookupInSymbol::customPerfectHash() const {
		auto hash_1 = base::perfectHash(symbol);
		auto hash_2 = std::hash<base::StrId>()(name);

		// @FIXME: this does not work:
		return (hash_1 * 143 + hash_2 * 7) * 2 + follow_wildcards;
	}

	struct ImplementationOf_QueryDealias: query::QueryImplementation<QueryDealias, SymbolList> {
		static auto provide(Context& ctx, QKey key) -> PResult {
			if (kind(key) != SymbolKind::Alias) return { key };

			auto&& x         = dynamic_cast<const pst::Alias*>(getSymRef(key)->pst_stmt.get());
			auto&& key_scope = scope(key);

			bool       first_symbol = true;
			SymbolList result;
			for (auto&& pointed: x->getPointed()) {
				auto&& pointed_symbol_lookup
					= first_symbol
				        ? ctx.query<QueryLookupInScopeAndParents>({ key_scope, pointed, false })
				        : ctx.query<QueryLookupInSymbol>({ result.back(), pointed, false });

				for (auto&& path = pointed_symbol_lookup.getAsSingle(); auto&& path_symbol: path)
					result.push_back(path_symbol);

				first_symbol = false;
			}

			return result;
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryDealias, "QueryDealias");
}
