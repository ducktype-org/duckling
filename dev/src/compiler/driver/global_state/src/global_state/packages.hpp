#pragma once

#include <base/str/string_id.hpp>

#include <filesystem/file.hpp>

#include <string>
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
		void addPackage(const std::string& name, const fs::FilePath& path);

		/** Adds the main package to the global state. */
		void addMainPackage(const std::string& name, const fs::FilePath& path);

		/** Test helper: clear registered packages so tests can reinitialize. */
		void clearPackagesForTests();
	}
}
