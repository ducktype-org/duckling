#include "standard_packages.hpp"

#include <frontend/module_tree/queries.hpp>

#include <algorithm>

namespace compiler::frontend::packages {

	const std::vector<base::StrID>& standardLibraryPackageIds() {
		static const std::vector ids{ base::StrID("core"), base::StrID("std") };
		return ids;
	}

	bool isStandardLibraryPackage(base::StrID package_id) {
		const auto& ids = standardLibraryPackageIds();
		return std::ranges::find(ids, package_id) != ids.end();
	}

	std::vector<ModuleID> standardLibraryRootModules(query::Context& ctx) {
		std::vector<ModuleID> root_modules;
		for (const auto& package_id: standardLibraryPackageIds()) {
			auto module = getModuleByAbsolutePath(ctx, package_id, {});
			if (module.has_value()) root_modules.push_back(module.value());
		}
		return root_modules;
	}
}
