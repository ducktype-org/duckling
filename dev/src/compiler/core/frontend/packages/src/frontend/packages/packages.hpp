#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>
#include <string_id/string_id.hpp>

#include <json/diagnostics.hpp>
#include <nlohmann/json_fwd.hpp>

#include <vector>

namespace compiler::frontend::packages {
	/**
	 * @brief Diagnostics for package parsing and resolution.
	 */
	using DiagnosticReporter = js::DiagnosticLogger;

	/**
	 * @brief Information about a package dependency in the package graph.
	 */
	struct PackageDependencyInfo final {
		/** @brief The ID of the dependency package. */
		base::StrID package_id;
		/**
		 * @brief Name used in importing code.
		 * Defaults to the package ID when no explicit alias is set.
		 */
		base::StrID alias;
	};

	/**
	 * @brief Resolved information about a single package.
	 * @note The package_id is stored in @c root_module's ModuleID.
	 */
	struct PackageInfo final {
		/** @brief The root module of the package. */
		compiler::frontend::ModuleID root_module;

		/** @brief Version of the package (currently informational). */
		base::StrID version;

		/** @brief Feature flags advertised by the package (currently informational). */
		std::vector<base::StrID> package_features;

		/** @brief Dependencies the package imports. */
		std::vector<PackageDependencyInfo> dependencies;
	};

	/**
	 * @brief Describes a single external dependency parsed from manifest input.
	 */
	struct RawDependencyInfo final {
		/** @brief The name of the dependency package. Used as @c package_id later. */
		base::StrID package_name;

		/** @brief Alias of the dependency used in importing code. */
		base::StrID alias;

		/**
		 * @brief Parse a dependency entry from JSON.
		 * @param report Reporter that receives any encountered diagnostics.
		 * @return The parsed dependency, or empty if a fatal error was reported.
		 */
		static base::Optional<RawDependencyInfo> fromJson(
			const nlohmann::json& json, const DiagnosticReporter& report
		);
	};

	/**
	 * @brief Describes a single package parsed from manifest input.
	 */
	struct RawPackageInfo final {
		/** @brief The name of the package. Used as @c package_id later. */
		base::StrID package_name;

		/** @brief Version of the package. */
		base::StrID version;

		/** @brief File path to the package's source root. */
		fs::FilePath package_path;

		/** @brief Feature flags advertised by the package. */
		std::vector<base::StrID> features;

		/** @brief Direct dependencies declared by the package. */
		std::vector<RawDependencyInfo> dependencies;

		/**
		 * @brief Parse a package entry from JSON.
		 * @param report Reporter that receives any encountered diagnostics.
		 * @return The parsed package, or empty if a fatal error was reported.
		 */
		static base::Optional<RawPackageInfo> fromJson(
			const nlohmann::json& json, const DiagnosticReporter& report
		);
	};

	/**
	 * @brief Resolve a RawPackageInfo into a PackageInfo by loading its module tree.
	 * Pure — does not log; routes errors via @p report.
	 */
	base::Optional<PackageInfo> createPackageInfo(
		const RawPackageInfo& package_info, const DiagnosticReporter& report
	);

}  // namespace compiler::frontend::packages
