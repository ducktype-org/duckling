#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <query_framework/context/context_fd.hpp>
#include <string_id/string_id.hpp>

#include <array>
#include <vector>

namespace compiler::frontend::packages {

	/**
	 * @brief The identifiers of the standard-library packages.
	 *
	 * This is the single source of truth for which packages make up the standard library.
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
