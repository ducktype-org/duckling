#include "queries.hpp"

#include "functors.hpp"
#include "module_tree.hpp"

#include <frontend/module_tree/access.hpp>
#include <global_state/packages.hpp>

#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include "query_framework/context/context.hpp"
#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

#include <query_framework/query_errors.hpp>

namespace compiler::frontend {

	namespace {
		ModuleID getRootAncestorModuleID(query::Context& ctx, ModuleID module_id) {
			auto current = module_id;
			while (auto parent = ctx.query<QueryParentModule>(current)) current = parent.value();
			return current;
		}

		const global_state::PackageInfo& getPackageInfo(query::Context& ctx, ModuleID module_id) {
			auto root_ancestor = getRootAncestorModuleID(ctx, module_id);
			const auto& all_packages = global_state::getPackages();
			for (const auto& pkg : all_packages)
				if (pkg.root_module == root_ancestor) return pkg;
			CORE_PANIC("Root module does not belong to any package");
		}

		const global_state::PackageInfo& getPackageInfo(base::StrID package_id) {
			const auto& all_packages = global_state::getPackages();
			for (const auto& pkg : all_packages)
				if (getModuleRef(pkg.root_module)->getPackageID() == package_id) return pkg;
			CORE_PANIC("Package not found");
		}
	}

	base::Optional<ModuleID> getRelativeModule(
		query::Context& ctx, ModuleID from, const std::vector<base::StrID>& path
	) {
		CORE_ASSERT(path.size() >= 1, "Empty module path");

		// @TODO: ambiguities

		// first step (in priority):
		// * check children
		// * check ancestors
		// * check package dependencies

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
			// Get the package info of the current module package and look for the dependency with the alias same as imported name.
			const auto& package_info = getPackageInfo(ctx, from);
			for (const auto& dep : package_info.dependencies) {
				if(path.at(0) == dep.alias) {
					current_module = getPackageInfo(dep.package_id).root_module;
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
