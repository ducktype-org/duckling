#include "scopes.hpp"
#include "../hout/hout.hpp"
#include "base/stable_container.hpp"
#include "query_framework/acd.hpp"
#include <base/string_id.hpp>
#include <query_framework/query_impl.hpp>

namespace compiler::helios {

	struct ScopeData {
		// adapted from hir:
		
		// created on startup:
		ScopeID parent;
		base::StrId name; ///< for debug
		bool is_root = false;

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
				.name = base::StrId("ROOT"),
				.is_root = true,
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

	// impl of simple getters ("non-query query"):
	// get name
	// debug print
	// etc 

	// if somewhere then here it is needed to handle cycles somehow
}


