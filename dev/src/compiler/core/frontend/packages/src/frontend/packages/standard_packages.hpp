// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <query_framework/context/context_fd.hpp>
#include <string_id/string_id.hpp>

#include <vector>

namespace compiler::frontend::packages {
	struct SharedDVMLibName {
		std::string soname{};
	};

	/**
	 * @brief A standard-library package: its id and the other standard-library packages it depends
	 * on. On-disk layout (the package lives in a directory named after its id under the std root)
	 * and artifact names are derived from the id by the driver.
	 */
	struct StandardLibraryPackage final {
		base::StrID              id;
		std::vector<base::StrID> dependencies;

		/**
		 * @brief Shared libs, only relevant for the DVM backend.
		 */
		std::vector<SharedDVMLibName> dvm_shared_libs{};

		[[nodiscard]] std::vector<std::string> getSharedLibsAsStr() const;
	};

	/**
	 * @brief The standard-library packages, in dependency order (a package's dependencies appear
	 * before it).
	 *
	 * This is the single source of truth for which packages make up the
	 * core and the standard library and how they depend on one another.
	 */
	const std::vector<StandardLibraryPackage>& standardLibraryPackages();

	/**
	 * @brief The identifiers of the standard-library packages, derived from
	 * `standardLibraryPackages()`.
	 */
	const std::vector<base::StrID>& standardLibraryPackageIds();

	/**
	 * @brief Whether @p package_id names one of the standard-library packages.
	 */
	bool isStandardLibraryPackage(base::StrID package_id);

	/**
	 * @brief The root modules of the currently-registered standard-library packages, for looking up
	 * symbols/modules within them. Packages that are not registered (e.g. under `--no-std`, or a
	 * custom std that lacks one) are skipped, so the result may be shorter than
	 * `standardLibraryPackageIds()`.
	 */
	std::vector<ModuleID> standardLibraryRootModules(query::Context& ctx);
}
