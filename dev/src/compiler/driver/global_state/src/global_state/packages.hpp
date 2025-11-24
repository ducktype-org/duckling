#pragma once

#include <string_id/string_id.hpp>

#include <filesystem/file.hpp>

#include <string_view>
#include <vector>

namespace global_state {

	/**
	 * Global state for managing package information.
	 */
	struct PackageInfo {
		base::StrID  package_name;
		fs::FilePath package_path;
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
		void addPackage(std::string_view name, const fs::FilePath& path);

		/** Adds the main package to the global state. */
		void addMainPackage(std::string_view name, const fs::FilePath& path);
	}
}
