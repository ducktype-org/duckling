#include "queries.hpp"

#include "module_tree.hpp"

#include <frontend/module_tree/functors.hpp>

#include <base/string_id.hpp>

#include <query_framework/query_impl.hpp>

namespace compiler::frontend {

	/*******************
	 * QueryModuleTree *
	 *******************/
	struct IMPLEMENT_QUERY(QueryModuleTree, compiler::frontend::ModuleID) {
		static auto provide(Context&, const QKey& key) -> PResult {
			Ref<ModuleTree> module_tree = ModuleTreeBuilder::create(key);

			return GetModuleID_Functor::make(module_tree);
		}

		QUERY_AUTO_CACHE_COPY
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
