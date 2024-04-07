#include <query_framework/query_impl.hpp>
#include <base/string_id.hpp>
#include "queries.hpp"

// @TODO: decide what we do with it
// NOLINTBEGIN(performance-unnecessary-value-param)

namespace compiler::frontend {

	/*******************
	 * QueryModuleTree *
	 *******************/
	struct ImplementationOf_QueryModuleTree:
		  query::QueryImplementation<QueryModuleTree, compiler::frontend::ModuleId> {
		inline static base::HashMap<QKey, query::AddACD<QResult>> cache{};

		static auto provide([[maybe_unused]] Context& context, QKey key) -> PResult {
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
			cache.put(key, { res, acd });
			return res;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryModuleTree, "QueryModuleTree");

	/*******************
	 * QuerySourceFile *
	 *******************/
	struct ImplementationOf_QuerySourceFile:
		  query::QueryImplementation<QueryFileID, compiler::frontend::FileId> {
		inline static base::HashMap<QKey, query::AddACD<QResult>> cache{};

		static auto provide([[maybe_unused]] Context& context, QKey key) -> PResult {
			SourceFile file = SourceFile(key);

			return file.id;
		}

		static auto load(QKey key) -> LoadResult {
			if (cache.contains(key))
				return cache.at(key);
			else
				return {};
		}

		static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
			cache.put(key, { res, acd });
			return res;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QuerySourceFile, "QuerySourceFile");
}

// NOLINTEND(performance-unnecessary-value-param)
