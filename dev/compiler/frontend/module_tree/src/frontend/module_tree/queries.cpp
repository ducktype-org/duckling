#include "queries.hpp"

#include "module_tree.hpp"

#include <query_framework/query_impl.hpp>

#include <base/string_id.hpp>

// @TODO: decide what we do with it
// NOLINTBEGIN(performance-unnecessary-value-param)

namespace compiler::frontend {

	/*******************
	 * QueryModuleTree *
	 *******************/
	struct IMPLEMENT_QUERY(QueryModuleTree, compiler::frontend::ModuleID) {
		inline static base::HashMap<QKey, query::CacheEntry<QResult>> cache{};

		static auto provide(Context&, QKey key) -> PResult {
			std::shared_ptr<ModuleTree> module_tree = ModuleTree::create(key);

			return module_tree->getID();
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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleTree);

	base::Optional<ModuleID> getRelativeModule(
		query::Context& ctx, ModuleID from, const std::vector<base::StrID>& path
	) {
		CORE_ASSERT(path.size() >= 1, "Empty module path");

		// @TODO: ambiguities

		// first step (in priority):
		// * check children
		// * check ancestors

		base::Optional<ModuleID> current_module;

		for (const auto& [name, submodule]: *ctx.query<QuerySubmodules>(from)) {
			if (name == path.at(0)) {
				current_module = submodule;
				break;
			}
		}
		if (not current_module.has_value()) {
			base::Optional<ModuleID> ancestor = ctx.query<QueryParentModule>(from);
			while (ancestor) {
				if (frontend::moduleName(ancestor.value()) == path.at(0)) {
					current_module = ancestor.value();
					break;
				}
				ancestor = ctx.query<QueryParentModule>(ancestor.value());
			}
		}

		// second step: follow children

		for (usize i = 1; i < path.size() and current_module.has_value(); i++) {
			auto curr_children = ctx.query<QuerySubmodules>(current_module.value());
			for (const auto& [name, submodule]: *curr_children) {
				if (name == path.at(i)) {
					current_module = submodule;
					break;
				}
			}
		}

		return current_module;
	}

}

// NOLINTEND(performance-unnecessary-value-param)
