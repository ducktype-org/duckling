#include "queries.hpp"

#include "functors.hpp"
#include "module_tree.hpp"

#include <frontend/module_tree/access.hpp>
#include <global_state/packages.hpp>

#include <base/collections/optional.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

namespace compiler::frontend {

	base::Optional<ModuleID> getRelativeModule(
		query::Context& ctx, ModuleID from, const std::vector<base::StrID>& path
	) {
		CORE_ASSERT(path.size() >= 1, "Empty module path");

		// @TODO: ambiguities

		// Resolve imports in two stages:
		// 1) Resolve path[0] in nearest scope order:
		//    local children -> (REPL/script only) ancestor children -> ancestor itself.
		// 2) Resolve path[1..] by descending through children only.

		base::Optional<ModuleID> current_module;

		auto get_next_ancestor = [&](ModuleID module_id) -> base::Optional<ModuleID> {
			auto parent = ctx.query<QueryParentModule>(module_id);
			if (parent.has_value()) return parent;

			// Synthetic REPL/script modules expose ancestry through REPL parent links.
			if (ctx.query<QueryIsReplModule>(module_id))
				return ctx.query<QueryReplModuleParent>(module_id);

			return {};
		};

		auto try_find_child
			= [&](ModuleID module_id, base::StrID child_name) -> base::Optional<ModuleID> {
			auto maybe_child = getModuleRef(module_id)->getSubmoduleByName(child_name).unlock(ctx);
			if (!maybe_child.has_value()) return {};
			return maybe_child.value().unlock(ctx).getID();
		};

		// 1) Try local child first.
		current_module = try_find_child(from, path.at(0));

		if (not current_module.has_value() && ctx.query<QueryIsReplModule>(from)) {
			// 2) Try ancestor children.
			base::Optional<ModuleID> ancestor = get_next_ancestor(from);
			while (ancestor.has_value() && !current_module.has_value()) {
				current_module = try_find_child(ancestor.value(), path.at(0));
				ancestor       = get_next_ancestor(ancestor.value());
			}
		}

		if (not current_module.has_value()) {
			// 3) Legacy fallback: allow matching an ancestor by name.
			base::Optional<ModuleID> ancestor = get_next_ancestor(from);
			while (ancestor.has_value()) {
				if (frontend::moduleName(ancestor.value()) == path.at(0)) {
					current_module = ancestor.value();
					break;
				}
				ancestor = get_next_ancestor(ancestor.value());
			}
		}

		if (not current_module.has_value()) {
			// Look up the dependency by alias in the current module's package.
			// Uses getPackageDependencyByAlias so we register a dependency only on this specific
			// (package, alias) edge instead of on every dependency of the package.
			auto owner_pkg_id  = getModuleRef(from)->getPackage().unlock(ctx).getID();
			auto owner_pkg_opt = global_state::getPackageRefOpt(owner_pkg_id);

			// The only situation where owner_pkg_opt would not have value is in the tests.
			if (not owner_pkg_opt.has_value()) return {};
			auto owner_pkg = owner_pkg_opt.value();

			if (owner_pkg->getName() == path.at(0)) {
				current_module = owner_pkg->getRootModule().unlock(ctx).getID();
			} else {
				auto dep_locked_opt
					= owner_pkg->getPackageDependencyByAlias(path.at(0)).unlock(ctx);
				if_opt_some(dep_locked_opt, dep_locked) {
					auto dep_pkg_id       = dep_locked.unlock(ctx).getID();
					auto dep_package_info = global_state::getPackageRef(dep_pkg_id);
					current_module        = dep_package_info->getRootModule().unlock(ctx).getID();
				}
			}
		}

		// Resolve remaining components only through descendants.
		for (usize i = 1; i < path.size() and current_module.has_value(); i++) {
			auto maybe_child2
				= getModuleRef(current_module.value())->getSubmoduleByName(path.at(i)).unlock(ctx);
			if (maybe_child2.has_value())
				current_module = maybe_child2.value().unlock(ctx).getID();
			else
				current_module.reset();
		}

		return current_module;
	}
}
