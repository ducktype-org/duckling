#pragma once

#include <base/collections/optional.hpp>

#include <hashing/component_hash.hpp>
#include <query_framework/context/context_fd.hpp>
#include <query_framework/utils/query_hash.hpp>
#include <string_id/string_id.hpp>

#include <cstddef>
#include <ostream>
#include <span>
#include <vector>

namespace compiler::frontend::packages {

	class PackageInfo;

	/**
	 * @brief Key for package side input query.
	 * Stores pre-computed package hash for performance, analogous to KeyOf_ModuleSideInput.
	 */
	struct KeyOf_PackageSideInput final {
		hashing::ComponentHash::HashType package_hash;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const;
		bool operator==(const KeyOf_PackageSideInput&) const = default;
	};

	/**
	 * @brief Key for package-dependency-by-alias side input query.
	 * Identifies a (package, alias) lookup edge: which package an alias resolves to (if any)
	 * inside @c package_hash. The @c found / @c target_package_id fields are part of the key
	 * so that distinct outcomes (alias missing / present-pointing-to-X / present-pointing-to-Y)
	 * are tracked as independent dependencies.
	 * Stored as metadata so it can be re-created during driver initialization, which is what
	 * puts it in the stream.
	 */
	struct KeyOf_PackageDependencyAliasSideInput final {
		hashing::ComponentHash::HashType package_hash;       //< owner package hash
		base::StrID                      alias;              //< alias being looked up
		bool                             found;              //< whether alias is declared
		base::Optional<base::StrID>      target_package_id;  //< target package id if found

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const;
		bool operator==(const KeyOf_PackageDependencyAliasSideInput&) const = default;

		void prettyPrint(std::ostream& os) const;
	};

	class PackageAccessLocked;
	class PackageDependencyAccessLocked;
	class PackageDependenciesAccessLocked;

	/**
	 * @brief Key for package dependency count side input query.
	 * Identifies the (owner package, dependency count) pair so that queries depending on
	 * the full dependency list of a package are invalidated when that count changes.
	 */
	struct KeyOf_PackageDependencyCountSideInput final {
		hashing::ComponentHash::HashType stable_hash;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const;
		bool operator==(const KeyOf_PackageDependencyCountSideInput&) const = default;

		/** @brief Compute the stable hash for a package's dependency-count side input. */
		[[nodiscard]] static KeyOf_PackageDependencyCountSideInput computeHash(
			base::StrID package_id, usize count
		);
	};

	/**
	 * @brief Unlocked access to a package.
	 * Holds the package id which can be used to fetch the PackageInfo via global state.
	 */
	class PackageAccess final {
		base::StrID m_package_id;
		friend class PackageAccessLocked;

		explicit PackageAccess(base::StrID package_id): m_package_id(package_id) {}

	public:
		[[nodiscard]] base::StrID getID() const { return m_package_id; }
	};

	/**
	 * @brief Wrapper around a package id that requires unlocking via Context to access.
	 * Unlocking registers a dependency on the package (QueryPackageSideInput).
	 */
	class PackageAccessLocked final {
		base::StrID m_package_id;

	public:
		explicit PackageAccessLocked(base::StrID package_id): m_package_id(package_id) {}

		/** @brief Unlock and register QueryPackageSideInput dependency. */
		[[nodiscard]] PackageAccess unlock(query::Context& ctx) const;

		/** @brief Access without registering dependency — only outside of queries / debug. */
		[[nodiscard]] PackageAccess illegalAccess() const { return PackageAccess(m_package_id); }
	};

	/**
	 * @brief Unlocked access to a single resolved package dependency.
	 * Exposes the alias and a PackageAccessLocked for the target package — the caller must
	 * unlock the latter separately if they want to register a dependency on the target
	 * package's identity (mirrors PackageDependencyAliasAccessLocked::unlock).
	 */
	class PackageDependencyAccess final {
		base::StrID m_alias;
		base::StrID m_target_package_id;
		friend class PackageDependencyAccessLocked;

		PackageDependencyAccess(base::StrID alias, base::StrID target_package_id):
			  m_alias(alias),
			  m_target_package_id(target_package_id) {}

	public:
		[[nodiscard]] base::StrID getAlias() const { return m_alias; }

		[[nodiscard]] PackageAccessLocked getPackage() const {
			return PackageAccessLocked(m_target_package_id);
		}
	};

	/**
	 * @brief Locked access to a single resolved package dependency.
	 * Returned (in batches) by PackageDependenciesAccessLocked::unlock. Unlocking registers
	 * QueryPackageDependencyAliasSideInput on the (owner_package, alias) → target package id
	 * edge and yields a PackageDependencyAccess.
	 */
	class PackageDependencyAccessLocked final {
		base::StrID m_owner_package_id;
		base::StrID m_alias;
		base::StrID m_target_package_id;

	public:
		PackageDependencyAccessLocked(
			base::StrID owner_package_id, base::StrID alias, base::StrID target_package_id
		):
			  m_owner_package_id(owner_package_id),
			  m_alias(alias),
			  m_target_package_id(target_package_id) {}

		/** @brief Unlock and register QueryPackageDependencyAliasSideInput dependency. */
		[[nodiscard]] PackageDependencyAccess unlock(query::Context& ctx) const;

		/** @brief Access without registering dependency — only outside of queries / debug. */
		[[nodiscard]] PackageDependencyAccess illegalAccess() const {
			return { m_alias, m_target_package_id };
		}
	};

	/**
	 * @brief AccessLocked over the full list of a package's dependencies.
	 * On unlock registers QueryPackageDependencyCountSideInput so that queries depending on
	 * the full dependency list are invalidated when its size changes. Returns a vector of
	 * PackageDependencyAccessLocked. Mirrors SubmodulesAccessLocked.
	 */
	class PackageDependenciesAccessLocked final {
		base::StrID                                m_owner_package_id;
		std::vector<PackageDependencyAccessLocked> m_dependencies;

	public:
		PackageDependenciesAccessLocked(
			base::StrID owner_package_id, std::vector<PackageDependencyAccessLocked> dependencies
		);

		[[nodiscard]] std::vector<PackageDependencyAccessLocked> unlock(query::Context& ctx) const;
		[[nodiscard]] std::vector<PackageDependencyAccessLocked> illegalAccess() const;
	};

	/**
	 * @brief AccessLocked for a package-dependency lookup by alias.
	 * Unlocking registers QueryPackageDependencyAliasSideInput recording whether the alias
	 * exists in the owner package and (if so) which package id it resolves to.
	 * Returns an Optional<PackageAccessLocked> — the caller decides whether to unlock further.
	 */
	class PackageDependencyAliasAccessLocked final {
		base::StrID                 m_owner_package_id;
		base::StrID                 m_alias;
		base::Optional<base::StrID> m_dependency_package_id;

	public:
		PackageDependencyAliasAccessLocked(
			base::StrID                 owner_package_id,
			base::StrID                 alias,
			base::Optional<base::StrID> dependency_package_id
		):
			  m_owner_package_id(owner_package_id),
			  m_alias(alias),
			  m_dependency_package_id(dependency_package_id) {}

		[[nodiscard]] base::Optional<PackageAccessLocked> unlock(query::Context& ctx) const;
		[[nodiscard]] base::Optional<PackageAccessLocked> illegalAccess() const;
	};

}  // namespace compiler::frontend::packages
