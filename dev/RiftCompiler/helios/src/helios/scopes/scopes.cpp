#include "scopes.hpp"
#include "../hout/hout.hpp"
#include "base/maps.hpp"
#include "base/stable_container.hpp"
#include "../lookup_result.hpp"
#include "base/str_concat.hpp"
#include "frontend/module_tree/module_tree.hpp"
#include "frontend/module_tree/queries.hpp"
#include "pst_parser/rift_parser_base.hpp"
#include "../pst_walkers.hpp"
#include "query_framework/acd.hpp"
#include <base/string_id.hpp>
#include <query_framework/query_impl.hpp>
#include "../pst_walkers.hpp"
#include "../symbols/symbols.hpp"
#include <base/stable_hashmap.hpp>

namespace compiler::helios {

	struct ScopeData {
		// adapted from hir:
		
		// created on startup:
		ScopeID parent;
		// base::StrId name; ///< for debug
		bool is_root = false;
		StmtList stmt_list;

		// cache entries:
		// in the future we might need separation for: direct symbols, expanded symbols
		// in this system scope is no longer closed/open as we think of it as a pure-value object
		// any lookup in the scope requires calculation of symbols witch itself is done only once!
		base::Optional<query::AddACD<SymbolList> > symbols;


		// This delete is important, to prevent any copy of scope data:
		// ScopeData(const ScopeData&)            = delete;
		// ScopeData& operator=(const ScopeData&) = delete;

	};

	struct GetScopeRef_Functor {
		static auto get(ScopeID id) { return id.ref; }
	};
	auto getScopeRef(ScopeID id) { return GetScopeRef_Functor::get(id); }

	std::string dprint(ScopeID id) {
		auto ref = getScopeRef(id);
		std::string out = base::strConcat(
			"Stmt count: ",
			ref->stmt_list.size(),
			"\n"
		);
		return out;
	}

	namespace {
		base::StableVector<ScopeData> scope_table;

		template<class... T>
		auto putInScopeTable(T&&... args) {
			auto key = scope_table.emplaceBack(std::forward<T>(args)...);
			return scope_table.getRef(key).value();
		}
	}

	struct ImplementationOf_QuerySuperRootScope: query::QueryImplementation<QueryRootScopeOf, ScopeID> {
		inline static base::Map<frontend::ModuleId, query::AddACD<ScopeID>> cache;
		
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @TODO: dont just ignore other files...
			auto&& main_file = ctx.query<frontend::QueryMainSourceFile>(key);
			auto&& module_pst = ctx.query<frontend::QueryFilePST>(main_file);
			
			return putInScopeTable(ScopeData{
				.parent = ScopeID{nullptr},
				// .name = base::StrId("ROOT"),
				.is_root = true,
				.stmt_list = getChildStmtsOf(module_pst.getTopLevelElement()),
				.symbols = {},
			});
		}

		static auto load(QKey key) -> LoadResult {
			return cache.atMaybeCopy(key);
		}

		static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
			cache.put(key, {res, acd});
			return cache.at(key).data;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QuerySuperRootScope, "Query Super Root scope");


	struct ImplementationOf_QueryPrimaryCodeScopeFor: query::QueryImplementation<QueryPrimaryCodeScopeFor, ScopeID> {

		// @FIXME: this cache allows for inconsistency
		static inline base::HashMap<pst::PstID, query::AddACD<ScopeID>> cache;
		
		static auto provide(Context& ctx, QKey element) -> PResult {
			auto list_of_stmt = getChildStmtsOf(element.base_element);
			auto parent = scope(ctx.query<QuerySymbolOfSTMT>({element.parent, element.base_element}));
			return putInScopeTable(ScopeData{
				.parent = parent,
				.stmt_list = list_of_stmt,
				.symbols = {},
			});
		}

		static auto load(QKey key) -> LoadResult {
			return cache.atMaybeCopy(key.base_element->getID());
		}

		static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
			cache.put(key.base_element->getID(), {res, acd});
			return res;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryPrimaryCodeScopeFor, "Query Scope Of");


	// impl of simple getters ("non-query query"):
	// get name
	// debug print
	// etc 

	// if somewhere then here it is needed to handle cycles somehow


	struct ImplementationOf_QuerySymbolsInScope: public query::QueryImplementation<QuerySymbolsInScope, std::vector<SymID>> {
		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<SymID> out;
			for (const auto& stmt: key.ref->stmt_list) {
				auto sym_id = ctx.query<QuerySymbolOfSTMT>({key, stmt});
				out.emplace_back(sym_id);
			}
			return out;
		}


		// @OPT: use cache in ScopeData for super fast lookup    
		static auto load(QKey key) -> LoadResult { 
			if (auto&& cache = key.ref->symbols) {
				return QResWithACD{ cache->data, cache->acd };
			}
			return {};
		}

		static auto store([[maybe_unused]] QKey key, PResult p_res, [[maybe_unused]] query::ACD acd)
			-> QResult {
			key.ref->symbols.emplace(PResWithACD{ std::move(p_res), acd });
			return key.ref->symbols.value().data;
		}
	};


	base::HashT KeyOf_QueryPrimaryCodeScopeFor::customPerfectHash() const {
		auto hash_1 = base::perfectHash(parent);
		auto hash_2 = base_element->getID().asInt();

		// @FIXME: this does not work:
		return hash_1*143 + hash_2*7;
	}
}


