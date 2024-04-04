#include <query_framework/query_impl.hpp>
#include "queries.hpp"

namespace compiler::frontend {

	/*****************
	 * QUERY 1:      *
	 * GetModuleTree *
	 *****************/
	struct ImplementationOf_GetModuleTreeQuery: query::QueryImplementation<GetModuleTreeQuery, compiler::frontend::ModuleId> {
		inline static std::map<QKey, query::AddACD<QResult>> cache{};

		static auto provide(Context& context, QKey key) -> PResult {
			std::shared_ptr<ModuleTree> module_tree = ModuleTree::create(key);

			return module_tree->getId();
		}
		static auto load(QKey key) -> LoadResult {
			if (cache.contains(key))
				return cache.at(key);
			else
				return {};
		}
		static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
			cache.insert({ key, { res, acd } });
			return res;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_GetModuleTreeQuery, "GetModuleTreeQuery");

}

