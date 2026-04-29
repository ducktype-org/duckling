#pragma once

#include <frontend/module_tree/module_id.hpp>

#include "base/except/exceptions.hpp"
#include <base/collections/optional.hpp>
#include "string_id/string_id.hpp"

#include <vector>

namespace global_state {


	/**
	 * Information about a package dependency.
	 */
	struct PackageDependencyInfo {
		/**
		 * The ID of the package.
		 */
		base::StrID package_id;
		/**
		 * The alias for the package.
		 * Alias is the actual name used in imports and references to the package. 
		 * It can be the same as the package ID or different.
		 */
		base::StrID alias; 
	};


	/**
	 * Global state for managing package information.
	 */
	struct PackageInfo {
		/**
		 * The root module of the package.
		 * @note The package_id is stored in the root_module ModuleID
		 */
		compiler::frontend::ModuleID              root_module;

		/**
		 * Version of the package. It migh be used in the future as a global define.
		 * @note This field is currently unused in the actual compilation process.
		 */
		base::StrID version;

		/**
		 * A list of features supported by the package.
		 * Features are flags that can be used in the code for conditional compilation.
		 * @note This field is currently unused in the actual compilation process.
		 */
		std::vector<base::StrID> package_features;

		/**
		 * The list of dependencies for the package.
		 * Dependencies can be imported in a code like other local modules.
		 */
		std::vector<PackageDependencyInfo> dependencies;
	};

	/**
	 * Returns the list of registered top-level packages.
	 */
	const std::vector<PackageInfo>& getPackages();

	namespace setters {
		/**
		 * Adds a package to the global state.
		 */
		void addPackage(const PackageInfo& package_info);

		/**
		 * Adds a package with no dependencies to the global state.
		 * This is simple wrapper and should be used only if compiling single package with no dependencies, e.g. for testing purposes.
		 */
		void addPackage(compiler::frontend::ModuleID root_module);

		/**
		 * Removes a package from the global state by its root module ID.
		 */
		void removePackage(compiler::frontend::ModuleID root_module);
	}
}
