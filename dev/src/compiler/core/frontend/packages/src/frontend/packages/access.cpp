#include "access.hpp"

#include "packages.hpp"
#include "queries.hpp"

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
		hashing::addToHash(hasher, found);
		hashing::addToHash(hasher, target_package_id);
		return hasher.finalize();
	}

	std::vector<std::byte> KeyOf_PackageDependencyAliasSideInput::serialize() const {
		std::vector<std::byte> data;
		auto                   hash_ptr = reinterpret_cast<const std::byte*>(&package_hash);
		data.insert(data.end(), hash_ptr, hash_ptr + sizeof(hashing::ComponentHash::HashType));

		auto append_str = [&](base::StrID s) {
			auto size     = s.size();
			auto size_ptr = reinterpret_cast<const std::byte*>(&size);
			data.insert(data.end(), size_ptr, size_ptr + sizeof(size));
			auto str_ptr = reinterpret_cast<const std::byte*>(s.data());
			data.insert(data.end(), str_ptr, str_ptr + s.size());
		};
		append_str(alias);
		data.emplace_back(std::byte(found));
		append_str(target_package_id);
		return data;
	}

	KeyOf_PackageDependencyAliasSideInput KeyOf_PackageDependencyAliasSideInput::deserialize(
		std::span<const std::byte> data
	) {
		KeyOf_PackageDependencyAliasSideInput out;
		usize                                 offset = 0;

		CORE_ASSERT(
			data.size() >= sizeof(hashing::ComponentHash::HashType),
			"KeyOf_PackageDependencyAliasSideInput::deserialize: buffer too small for header"
		);
		std::memcpy(&out.package_hash, data.data(), sizeof(hashing::ComponentHash::HashType));
		offset += sizeof(hashing::ComponentHash::HashType);

		auto read_str = [&](base::StrID& dst) {
			CORE_ASSERT(
				data.size() >= offset + sizeof(usize),
				"KeyOf_PackageDependencyAliasSideInput::deserialize: buffer too small for str size"
			);
			usize len = 0;
			std::memcpy(&len, data.data() + offset, sizeof(usize));
			offset += sizeof(usize);
			CORE_ASSERT(
				data.size() >= offset + len,
				"KeyOf_PackageDependencyAliasSideInput::deserialize: buffer too small for str data"
			);
			auto* ptr = reinterpret_cast<const char*>(data.data() + offset);
			dst       = base::StrID({ ptr, len });
			offset += len;
		};
		read_str(out.alias);

		CORE_ASSERT(
			data.size() >= offset + sizeof(std::byte),
			"KeyOf_PackageDependencyAliasSideInput::deserialize: buffer too small for found flag"
		);
		auto found_byte = std::byte(0);
		std::memcpy(&found_byte, data.data() + offset, sizeof(std::byte));
		out.found = bool(found_byte);
		offset += sizeof(std::byte);

		read_str(out.target_package_id);
		return out;
	}

	void KeyOf_PackageDependencyAliasSideInput::prettyPrint(std::ostream& os) const {
		os << "KeyOf_PackageDependencyAliasSideInput {\n";
		os << "  package_hash: " << package_hash.toStringHex() << "\n";
		os << "  alias: " << alias.strView() << "\n";
		os << "  found: " << (found ? "true" : "false") << "\n";
		os << "  target_package_id: " << target_package_id.strView() << "\n";
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

	PackageAccess PackageAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QueryPackageSideInput>(KeyOf_PackageSideInput{
			PackageInfo::computeHash(m_package_id) });
		return PackageAccess(m_package_id);
	}

	base::Optional<PackageAccessLocked> PackageDependencyAliasAccessLocked::unlock(query::Context& ctx
	) const {
		ctx.query<QueryPackageDependencyAliasSideInput>(KeyOf_PackageDependencyAliasSideInput{
			.package_hash = PackageInfo::computeHash(m_owner_package_id),
			.alias        = m_alias,
			.found        = m_dependency_package_id.has_value(),
			.target_package_id
			= m_dependency_package_id.has_value() ? *m_dependency_package_id : base::StrID(),
		});
		if (!m_dependency_package_id.has_value()) return {};
		return PackageAccessLocked(*m_dependency_package_id);
	}

	base::Optional<PackageAccessLocked> PackageDependencyAliasAccessLocked::illegalAccess() const {
		if (!m_dependency_package_id.has_value()) return {};
		return PackageAccessLocked(*m_dependency_package_id);
	}

}  // namespace compiler::frontend::packages
