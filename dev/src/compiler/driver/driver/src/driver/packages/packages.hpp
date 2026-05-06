#pragma once
#include <global_state/packages.hpp>

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>
#include <string_id/string_id.hpp>

#include <nlohmann/json_fwd.hpp>

namespace compiler::driver {

	/**
	 * @brief Describes a single external dependency of the main package.
	 * This dependency info should be returned from parsing compilation options (e.g. from a
	 * manifest file). And transferred to global_state::DependencyInfo in the process of compiler
	 * initialization.
	 */
	struct RawDependencyInfo final {
		/**
		 * @brief The name of the dependency package.
		 * This name must be unique and it's used as package_id in global_state::DependencyInfo.
		 */
		base::StrID package_name;

		/**
		 * @brief Alias of the dependency used in some package code.
		 */
		base::StrID alias;

		static base::Optional<RawDependencyInfo> fromJson(const nlohmann::json& json);
	};

	/**
	 * @brief Describes a single package in the system.
	 * This package info should be returned from parsing compilation options (e.g. from a manifest
	 * file). And transferred to global_state::PackageInfo in the process of compiler
	 * initialization.
	 */
	struct RawPackageInfo final {
		/**
		 * @brief The name of the package.
		 * This name must be unique and it's used as package_id in global_state::PackageInfo.
		 */
		base::StrID package_name;

		/**
		 * @brief The version of the package.
		 * This field is currently unused in the actual compilation process, but it might be used in
		 * the future as a global define.
		 */
		base::StrID version;

		/**
		 * @brief The file path to the package.
		 * This path is used to locate the package on the filesystem and load its contents during
		 * compilation.
		 */
		fs::FilePath package_path;

		/**
		 * @brief A list of features supported by the package.
		 * Features are flags that can be used in the code for conditional compilation.
		 * This field is currently unused in the actual compilation process, but it might be used in
		 * the future for conditional compilation based on package features.
		 */
		std::vector<base::StrID> features;

		/**
		 * @brief A list of dependencies for the package.
		 * Dependencies can be imported in a code like other local modules.
		 * This field is used to determine which other packages need to be compiled and linked
		 * together with this package.
		 */
		std::vector<RawDependencyInfo> dependencies;

		/**
		 * @brief Creates a RawPackageInfo instance from a JSON object.
		 * @param json The JSON object containing the package information.
		 * @return The created RawPackageInfo instance or an empty optional if the JSON is invalid.
		 */
		static base::Optional<RawPackageInfo> fromJson(const nlohmann::json& json);
	};

	/**
	 * @brief Creates a global_state::PackageInfo instance from a RawPackageInfo instance.
	 * This involves loading the package's module tree and validating its structure.
	 * @param package_info The RawPackageInfo instance containing the raw package information.
	 * @note This function logs errors to the global logger if the package is invalid.
	 * @return The created global_state::PackageInfo instance or an empty optional if the package is
	 * invalid.
	 */
	base::Optional<global_state::PackageInfo> createGlobalPackageInfo(
		const RawPackageInfo& package_info
	);
}
