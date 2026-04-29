#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <vector>

namespace global_state {

	/**
	 * Global state for managing package information.
	 */
	struct PackageInfo {
		compiler::frontend::ModuleID root_module;
	};

	/**
	 * Returns the list of registered packages. Included the main package.
	 * @note The main package is always the first element in the returned vector.
	 */
	const std::vector<PackageInfo>& getPackages();

	/**
	 * Returns the main package info.
	 */
	const PackageInfo& getMainPackage();

	namespace setters {
		/**
		 * Adds a package to the global state.
		 */
		void addPackage(compiler::frontend::ModuleID root_module);

		/**
		 * Removes a package from the global state by its root module ID.
		 */
		void removePackage(compiler::frontend::ModuleID root_module);

		/** Adds the main package to the global state. */
		void addMainPackage(compiler::frontend::ModuleID root_module);
	}
}
