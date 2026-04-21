#include "queries.hpp"

#include "functors.hpp"
#include "module_tree.hpp"

#include <frontend/module_tree/access.hpp>
#include <global_state/packages.hpp>

#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

namespace compiler::frontend {

	base::Optional<ModuleID> getRelativeModule(
		query::Context& ctx, ModuleID from, const std::vector<base::StrID>& path
	) {
		CORE_ASSERT(path.size() >= 1, "Empty module path");

		// @TODO: ambiguities

		// first step (in priority):
		// * check children
		// * check ancestors
		// * check external packages

		base::Optional<ModuleID> current_module;

		auto maybe_child = getModuleRef(from)->getSubmoduleByName(path.at(0)).unlock(ctx);
		if (maybe_child.has_value()) current_module = maybe_child.value().unlock(ctx).getID();

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

		if (not current_module.has_value()) {
			auto& packages = global_state::getPackages();
			// This includes the main package it will never be selected, cos we already checked the
			// root podule in previous loop
			for (const auto& package: packages) {
				if (frontend::getModuleRef(package.root_module)->getPackageID() == path.at(0)) {
					current_module = package.root_module;
					break;
				}
			}
		}

		// second step: follow children
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
