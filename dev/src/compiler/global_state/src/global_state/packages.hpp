#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <vector>

namespace global_state {

	/**
	 * Global state for managing package information.
	 */
	struct PackageInfo {
		compiler::frontend::ModuleID root_module;
		std::vector<compiler::frontend::ModuleID> dependencies;
	};

	/**
	 * Returns the list of registered top-level packages.
	 */
	const std::vector<PackageInfo>& getPackages();

	/**
	 * @brief Returns package info that owns the given module.
	 * Finds a package only in the top-level packages vector by package-id.
	 */
	const PackageInfo& getCurrentPackageInfo(compiler::frontend::ModuleID module_id);

	/**
	 * @brief Returns all registered package root module IDs, including dependencies,
	 * sorted and deduplicated.
	 */
	std::vector<compiler::frontend::ModuleID>
	getAllPackagesWithDependenciesRootModulesSortedDeduplicated();

	namespace setters {
		/**
		 * Adds a package to the global state.
		 */
		void addPackage(const PackageInfo& package_info);

		/**
		 * Adds a package with no dependencies to the global state.
		 */
		void addPackage(compiler::frontend::ModuleID root_module);

		/**
		 * Removes a package from the global state by its root module ID.
		 */
		void removePackage(compiler::frontend::ModuleID root_module);
	}
}
