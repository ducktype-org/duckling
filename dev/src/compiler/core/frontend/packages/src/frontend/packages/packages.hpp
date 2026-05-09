#pragma once

#include "access.hpp"

#include <frontend/module_tree/access.hpp>
#include <frontend/module_tree/module_id.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file_path.hpp>
#include <hashing/component_hash.hpp>
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
	 *
	 * Identified by its package id (== root module's package id). Provides access to its
	 * dependencies through a query-friendly mechanism: getPackageDependencyByAlias returns
	 * a PackageDependencyAliasAccessLocked whose unlock registers a side input recording the
	 * (package, alias) → target package lookup result.
	 */
	class PackageInfo final {
	public:
		PackageInfo(
			compiler::frontend::ModuleID       root_module,
			base::StrID                        version,
			std::vector<base::StrID>           features,
			std::vector<PackageDependencyInfo> dependencies
		);

		/** @brief Package ID — equal to the root module's package id. */
		[[nodiscard]] base::StrID getPackageID() const;

		/** @brief Stable hash of the package, used as side-input key. */
		[[nodiscard]] const hashing::ComponentHash::HashType& getPackageHash() const;

		/**
		 * @brief Root module of the package as an AccessLocked wrapper.
		 * Unlock in queries to register a dependency on the module; use illegalAccess outside.
		 */
		[[nodiscard]] compiler::frontend::ModuleAccessLocked getRootModule() const;

		/** @brief Version of the package (currently informational, requires query dependency
		 * tracking). */
		[[nodiscard]] base::StrID getVersion() const;

		/** @brief Feature flags advertised by the package (currently informational, requires query
		 * dependency tracking). */
		[[nodiscard]] base::CRef<std::vector<base::StrID>> getFeatures() const;

		/**
		 * @brief Direct dependencies declared by the package.
		 * Mirrors ModuleTree::getSubmodules — on unlock registers
		 * QueryPackageDependencyCountSideInput so consumers depending on the dependency list
		 * are invalidated when its size changes; outside queries use illegalAccess.
		 */
		[[nodiscard]] PackageDependenciesAccessLocked getDependencies() const;

		/**
		 * @brief Lookup a dependency by its alias.
		 * Unlock registers QueryPackageDependencyAliasSideInput recording whether the alias
		 * exists and which package id it resolves to. Returns Optional<PackageAccessLocked>
		 * — empty if the alias is not declared. The caller decides whether to unlock further.
		 */
		[[nodiscard]] PackageDependencyAliasAccessLocked getPackageDependencyByAlias(base::StrID alias
		) const;

	private:
		friend class PackageAccessLocked;
		friend class PackageDependencyAliasAccessLocked;

		/** @brief Compute the stable hash of a package from its package id. */
		[[nodiscard]] static hashing::ComponentHash::HashType computeHash(base::StrID package_id);

		compiler::frontend::ModuleID       m_root_module;
		base::StrID                        m_version;
		std::vector<base::StrID>           m_features;
		std::vector<PackageDependencyInfo> m_dependencies;
		hashing::ComponentHash::HashType   m_hash;
	};

	/**
	 * @brief Describes a single external dependency parsed from manifest input.
	 */
	struct RawDependencyInfo final {
		/** @brief The name of the dependency package. Used as @c package_id later. */
		base::StrID package_name;

		/** @brief Alias of the dependency used in importing code. Empty if no explicit alias was
		 * provided (defaults to @c package_name). */
		base::Optional<base::StrID> alias;

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
	 * @return The loaded PackageInfo or empty if an error was reported.
	 */
	base::Optional<PackageInfo> createPackageInfo(
		const RawPackageInfo& package_info, const DiagnosticReporter& report
	);

}  // namespace compiler::frontend::packages
