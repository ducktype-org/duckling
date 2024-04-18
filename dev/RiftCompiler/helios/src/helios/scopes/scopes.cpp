#include "scopes.hpp"
#include "../hout/hout.hpp"
#include "base/maps.hpp"
#include "base/stable_container.hpp"
#include "../lookup_result.hpp"
#include "pst_parser/rift_parser_base.hpp"
#include "query_framework/acd.hpp"
#include <base/string_id.hpp>
#include <query_framework/query_impl.hpp>
#include "../pst_walkers.hpp"
#include "../symbols/symbols.hpp"

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


	namespace {
		base::StableVector<ScopeData> scope_table;

		template<class... T>
		auto putInScopeTable(T&&... args) {
			auto key = scope_table.emplaceBack(std::forward<T>(args)...);
			return scope_table.getRef(key).value();
		}
	}

	struct ImplementationOf_QuerySuperRootScope: query::QueryImplementation<QuerySuperRootScope, ScopeID> {
		inline static base::Optional<query::AddACD<ScopeID>> cache;
		
		static auto provide(Context&, QKey) -> PResult {
			return putInScopeTable(ScopeData{
				.parent = ScopeID{nullptr},
				// .name = base::StrId("ROOT"),
				.is_root = true,
				.stmt_list = {}, //<< TODO
				.symbols = {},
			});
		}

		static auto load(QKey) -> LoadResult {
			return cache;
		}

		static auto store(QKey, PResult res, query::ACD acd) -> QResult {
			cache = {res, acd};
			return cache.value().data;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QuerySuperRootScope, "Query Super Root scope");


	struct ImplementationOf_QueryPrimaryCodeScopeFor: query::QueryImplementation<QueryPrimaryCodeScopeFor, ScopeID> {

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
			if (auto data = cache.atMaybe(key.base_element->getID())) {
				return QResWithACD{ data->data, data->acd };
			}
			return {};
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
}


