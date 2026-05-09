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
#include <query_framework/query_errors.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

namespace compiler::frontend {

	namespace {
		base::Optional<compiler::frontend::packages::PackageInfo> getPackageInfo(
			base::StrID package_id
		) {
			const auto& all_packages = global_state::getPackages();
			for (const auto& pkg: all_packages)
				if (pkg.getPackageID() == package_id) return pkg;

			// The error is already logged, when verifying the manifest
			// This could be panic, but we allow the compiler to run with broken manifest, to
			// collect as many errors as possible.
			return {};
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
			// Look up the dependency by alias in the current module's package.
			// Uses getPackageDependencyByAlias so we register a dependency only on this specific
			// (package, alias) edge instead of on every dependency of the package.
			auto owner_pkg_id  = getModuleRef(from)->getPackageID().unlock(ctx).getPackageID();
			auto owner_pkg_opt = getPackageInfo(owner_pkg_id);
			if (not owner_pkg_opt.has_value()) return {};

			auto dep_locked_opt
				= owner_pkg_opt.value().getPackageDependencyByAlias(path.at(0)).unlock(ctx);
			if_opt_some(dep_locked_opt, dep_locked) {
				auto dep_pkg_id           = dep_locked.unlock(ctx).getPackageID();
				auto dep_package_info_opt = getPackageInfo(dep_pkg_id);
				if_opt_some(dep_package_info_opt, dep_package_info) {
					current_module = dep_package_info.getRootModule().unlock(ctx).getID();
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
