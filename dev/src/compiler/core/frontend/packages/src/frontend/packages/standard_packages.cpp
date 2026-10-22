// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "standard_packages.hpp"

#include <frontend/module_tree/queries.hpp>

#include <os_utils/system_libraries.hpp>

#include <algorithm>
#include <ranges>

namespace compiler::frontend::packages {

	const std::vector<StandardLibraryPackage>& standardLibraryPackages() {
		static const std::vector<StandardLibraryPackage> packages{
			// `core` declares the C standard library FFI symbols, so the libraries providing
			// them have to be loaded by anything linking it.
			{
				.id              = base::StrID("core"),
				.dependencies    = {},
				.dvm_shared_libs = { SharedDVMLibName{os_utils::systemSharedLibC()}, SharedDVMLibName{os_utils::systemSharedLibM()}, },
			},
			{
				.id              = base::StrID("std"),
				.dependencies    = { base::StrID("core") },
				.dvm_shared_libs = {},
			},
		};
		return packages;
	}

	const std::vector<base::StrID>& standardLibraryPackageIds() {
		static const std::vector<base::StrID> ids
			= standardLibraryPackages() | std::views::transform(&StandardLibraryPackage::id)
		    | std::ranges::to<std::vector>();
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

	std::vector<std::string> StandardLibraryPackage::getSharedLibsAsStr() const {
		return dvm_shared_libs | std::views::transform(&SharedDVMLibName::soname)
		     | std::ranges::to<std::vector>();
	}
}
