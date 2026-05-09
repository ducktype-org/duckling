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
		ModuleID getRootAncestorModuleID(query::Context& ctx, ModuleID module_id) {
			auto current = module_id;
			while (auto parent = ctx.query<QueryParentModule>(current)) current = parent.value();
			return current;
		}

		base::Optional<compiler::frontend::packages::PackageInfo> getPackageInfo(
			query::Context& ctx, ModuleID module_id
		) {
			auto        root_ancestor = getRootAncestorModuleID(ctx, module_id);
			const auto& all_packages  = global_state::getPackages();
			for (const auto& pkg: all_packages)
				if (pkg.getRootModule().illegalAccess().getID() == root_ancestor) return pkg;

			// This could be panic, but in some tests we might want to compile modules directly
			// without initializing the compiler
			return {};
		}

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
			// Get the package info of the current module package and look for the dependency with
			// the alias same as imported name.
			auto package_info_opt = getPackageInfo(ctx, from);
			if (not package_info_opt.has_value()) return {};

			for (const auto& dep: package_info_opt.value().getDependencies().illegalAccess()) {
				if (path.at(0) == dep.getAlias()) {
					auto dep_package_info_opt
						= getPackageInfo(dep.getPackage().illegalAccess().getPackageID());
					if_opt_some(dep_package_info_opt, dep_package_info) {
						current_module = dep_package_info.getRootModule().illegalAccess().getID();
					}
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
