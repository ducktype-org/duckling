#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <frontend/packages/packages.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <artifacts/artifacts_fd.hpp>
#include <string_id/string_id.hpp>

#include <vector>

namespace global_state {

	/**
	 * @brief Returns the list of registered top-level packages.
	 */
	const std::vector<compiler::frontend::packages::PackageInfo>& getPackages();

	/**
	 * @brief Returns the PackageInfo for a given package ID.
	 * @return The PackageInfo associated with the provided package ID or empty optional if no
	 * such package exists.
	 */
	base::Optional<base::CRef<compiler::frontend::packages::PackageInfo>> getPackageRefOpt(
		base::StrID package_id
	);

	/**
	 * @brief Returns the PackageInfo for a given package ID.
	 * @note Panics if no package with the given name exists.
	 */
	base::CRef<compiler::frontend::packages::PackageInfo> getPackageRef(base::StrID package_id);

	/**
	 * @brief Get the std custom artifact collection for the stdlib binaries.
	 */
	base::Optional<base::Ref<artifacts::ArtifactCollection>> getStdArtifactsCollection();

	namespace setters {
		/** @brief Adds a package to the global state. */
		void addPackage(const compiler::frontend::packages::PackageInfo& package_info);

		/**
		 * @brief Adds a package with no dependencies (single-package convenience).
		 */
		void addPackage(compiler::frontend::ModuleID root_module);

		/** @brief Removes a package by its root module ID. */
		void removePackage(compiler::frontend::ModuleID root_module);

		/** @brief Set the custom artifact collection for the stdlib binaries.  */
		void setCustomStdArtifactsCollection(
			base::Box<artifacts::ArtifactCollection> custom_art_collection
		);
	}

}  // namespace global_state
