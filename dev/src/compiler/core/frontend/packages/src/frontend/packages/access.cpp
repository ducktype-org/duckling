#include "access.hpp"

#include "packages.hpp"

#include <queries.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/component_hash.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>

#include <cstring>

namespace compiler::frontend::packages {

	query::QueryStableHash KeyOf_PackageSideInput::queryStablePerfectHash() const {
		return package_hash;
	}

	query::QueryStableHash KeyOf_PackageDependencyCountSideInput::queryStablePerfectHash() const {
		return stable_hash;
	}

	KeyOf_PackageDependencyCountSideInput KeyOf_PackageDependencyCountSideInput::computeHash(
		base::StrID package_id, usize count
	) {
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, package_id);
		hashing::addToHash(hasher, static_cast<u64>(count));
		return KeyOf_PackageDependencyCountSideInput{ hasher.finalize() };
	}

	query::QueryStableHash KeyOf_PackageDependencyAliasSideInput::queryStablePerfectHash() const {
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, package_hash);
		hashing::addToHash(hasher, alias);
		hashing::addToHash(hasher, target_package_id.has_value());
		if (target_package_id.has_value()) hashing::addToHash(hasher, target_package_id.value());
		return hasher.finalize();
	}

	void KeyOf_PackageDependencyAliasSideInput::prettyPrint(std::ostream& os) const {
		os << "KeyOf_PackageDependencyAliasSideInput {\n";
		os << "  package_hash: " << package_hash.toStringHex() << "\n";
		os << "  alias: " << alias.strView() << "\n";
		os << "  found: " << (found ? "true" : "false") << "\n";
		if (target_package_id.has_value())
			os << "  target_package_id: " << target_package_id.value().strView() << "\n";
		os << "}\n";
	}

	IMPLEMENT_QUERY_SIDE_INPUT(QueryPackageSideInput);
	IMPLEMENT_QUERY_SIDE_INPUT(QueryPackageDependencyCountSideInput);

	IMPLEMENT_QUERY_SIDE_INPUT_WITH_LOGIC(QueryPackageDependencyAliasSideInput, ctx, key, {
		ctx.addMetadataIfNotExists<metadata_PackageDependencyAliasLookup>(key);
	});

	PackageDependenciesAccessLocked::PackageDependenciesAccessLocked(
		base::StrID owner_package_id, std::vector<PackageDependencyAccessLocked> dependencies
	):
		  m_owner_package_id(owner_package_id),
		  m_dependencies(std::move(dependencies)) {}

	std::vector<PackageDependencyAccessLocked> PackageDependenciesAccessLocked::unlock(
		query::Context& ctx
	) const {
		ctx.query<QueryPackageDependencyCountSideInput>(
			KeyOf_PackageDependencyCountSideInput::computeHash(
				m_owner_package_id, m_dependencies.size()
			)
		);
		return m_dependencies;
	}

	std::vector<PackageDependencyAccessLocked> PackageDependenciesAccessLocked::illegalAccess(
	) const {
		return m_dependencies;
	}

	PackageDependencyAccess PackageDependencyAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QueryPackageDependencyAliasSideInput>(KeyOf_PackageDependencyAliasSideInput{
			.package_hash      = PackageInfo::computeHash(m_owner_package_id),
			.alias             = m_alias,
			.found             = true,
			.target_package_id = m_target_package_id,
		});
		return illegalAccess();
	}

	PackageAccess PackageAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QueryPackageSideInput>(KeyOf_PackageSideInput{
			PackageInfo::computeHash(m_package_id) });
		return PackageAccess(m_package_id);
	}

	base::Optional<PackageAccessLocked> PackageDependencyAliasAccessLocked::unlock(query::Context& ctx
	) const {
		ctx.query<QueryPackageDependencyAliasSideInput>(KeyOf_PackageDependencyAliasSideInput{
			.package_hash      = PackageInfo::computeHash(m_owner_package_id),
			.alias             = m_alias,
			.found             = m_dependency_package_id.has_value(),
			.target_package_id = m_dependency_package_id,
		});
		if (!m_dependency_package_id.has_value()) return {};
		return PackageAccessLocked(*m_dependency_package_id);
	}

	base::Optional<PackageAccessLocked> PackageDependencyAliasAccessLocked::illegalAccess() const {
		if (!m_dependency_package_id.has_value()) return {};
		return PackageAccessLocked(*m_dependency_package_id);
	}

	void collectPackageInputData(
		const std::vector<PackageInfo>&          packages,
		bool                                     from_previous_metadata,
		std::vector<query::external::InputData>& out
	) {
		for (const auto& package: packages) {
			out.emplace_back(
				QueryPackageSideInput::getID(),
				KeyOf_PackageSideInput{ package.getPackageHash() }.queryStablePerfectHash()
			);
			out.emplace_back(
				QueryPackageDependencyCountSideInput::getID(),
				KeyOf_PackageDependencyCountSideInput::computeHash(
					package.getPackageID(), package.getDependencies().illegalAccess().size()
				)
					.queryStablePerfectHash()
			);
		}

		auto lookups
			= from_previous_metadata
		        ? query::external::getMetadataFromAllPrevNodes<metadata_PackageDependencyAliasLookup>(
				  )
		        : query::external::getMetadataFromAllCurrentNodes<
					  metadata_PackageDependencyAliasLookup>();

		for (const auto& lookup: lookups) {
			/* Metadata may be attached to other query types as well. */
			if (lookup.input_data.q_id != QueryPackageDependencyAliasSideInput::getID()) continue;
			const auto& key = lookup.value->value;

			const PackageInfo* owner = nullptr;
			for (const auto& package: packages)
				if (package.getPackageHash() == key.package_hash) {
					owner = &package;
					break;
				}
			/* Owner package no longer exists -> the lookup node stays red. */
			if (owner == nullptr) continue;

			auto       target = owner->getPackageDependencyByAlias(key.alias).illegalAccess();
			const bool found  = target.has_value();
			if (found != key.found) continue;
			if (found && target.value().illegalAccess().getID() != key.target_package_id.value())
				continue;

			/* Lookup result is unchanged -> reuse the stored InputData. */
			out.emplace_back(lookup.input_data);
		}
	}

} /* namespace compiler::frontend::packages */
