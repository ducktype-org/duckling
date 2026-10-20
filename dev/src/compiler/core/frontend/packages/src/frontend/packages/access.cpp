// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "access.hpp"

#include "packages.hpp"

#include <queries.hpp>

#include <base/collections/stable_hashmap.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/component_hash.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>

#include <cstring>
#include <string_view>
#include <unordered_set>

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

	query::QueryStableHash KeyOf_PackageNameSideInput::queryStablePerfectHash() const {
		return stable_hash;
	}

	KeyOf_PackageNameSideInput KeyOf_PackageNameSideInput::computeHash(
		base::StrID package_id, base::StrID name
	) {
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, package_id);
		hashing::addToHash(hasher, name);
		return KeyOf_PackageNameSideInput{ hasher.finalize() };
	}

	namespace {
		void appendStr(std::vector<std::byte>& data, base::StrID s) {
			auto size     = s.size();
			auto size_ptr = reinterpret_cast<const std::byte*>(&size);
			data.insert(data.end(), size_ptr, size_ptr + sizeof(size));
			auto str_ptr = reinterpret_cast<const std::byte*>(s.data());
			data.insert(data.end(), str_ptr, str_ptr + s.size());
		}

		base::StrID readStr(
			std::span<const std::byte> data, usize& offset, std::string_view key_name
		) {
			CORE_ASSERT(
				data.size() >= offset + sizeof(usize),
				key_name,
				"::deserialize: buffer too small for str size"
			);
			usize len = 0;
			std::memcpy(&len, data.data() + offset, sizeof(usize));
			offset += sizeof(usize);
			CORE_ASSERT(
				data.size() >= offset + len, key_name, "::deserialize: buffer too small for str data"
			);
			auto* ptr = data.data() + offset;
			offset += len;
			return base::StrID(base::RawView{ ptr, len });
		}

		bool readFlag(std::span<const std::byte> data, usize& offset, std::string_view key_name) {
			CORE_ASSERT(
				data.size() >= offset + sizeof(std::byte),
				key_name,
				"::deserialize: buffer too small for found flag"
			);
			auto flag_byte = std::byte(0);
			std::memcpy(&flag_byte, data.data() + offset, sizeof(std::byte));
			offset += sizeof(std::byte);
			return flag_byte != std::byte{ 0 };
		}
	}

	query::QueryStableHash KeyOf_PackageDependencyAliasSideInput::queryStablePerfectHash() const {
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, package_hash);
		hashing::addToHash(hasher, alias);
		hashing::addToHash(hasher, target_package_id.has_value());
		if (target_package_id.has_value()) hashing::addToHash(hasher, target_package_id.value());
		return hasher.finalize();
	}

	std::vector<std::byte> KeyOf_PackageDependencyAliasSideInput::serialize() const {
		std::vector<std::byte> data;
		auto                   hash_ptr = reinterpret_cast<const std::byte*>(&package_hash);
		data.insert(data.end(), hash_ptr, hash_ptr + sizeof(hashing::ComponentHash::HashType));

		appendStr(data, alias);
		data.emplace_back(std::byte(target_package_id.has_value()));
		if (target_package_id.has_value()) appendStr(data, target_package_id.value());
		return data;
	}

	KeyOf_PackageDependencyAliasSideInput KeyOf_PackageDependencyAliasSideInput::deserialize(
		std::span<const std::byte> data
	) {
		constexpr std::string_view KEY_NAME = "KeyOf_PackageDependencyAliasSideInput";

		KeyOf_PackageDependencyAliasSideInput out;
		usize                                 offset = 0;

		CORE_ASSERT(
			data.size() >= sizeof(hashing::ComponentHash::HashType),
			KEY_NAME,
			"::deserialize: buffer too small for header"
		);
		std::memcpy(&out.package_hash, data.data(), sizeof(hashing::ComponentHash::HashType));
		offset += sizeof(hashing::ComponentHash::HashType);

		out.alias = readStr(data, offset, KEY_NAME);
		out.found = readFlag(data, offset, KEY_NAME);
		if (out.found) out.target_package_id = readStr(data, offset, KEY_NAME);
		return out;
	}

	query::QueryStableHash KeyOf_PackageLookupSideInput::queryStablePerfectHash() const {
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, package_id);
		hashing::addToHash(hasher, found);
		return hasher.finalize();
	}

	std::vector<std::byte> KeyOf_PackageLookupSideInput::serialize() const {
		std::vector<std::byte> data;
		appendStr(data, package_id);
		data.emplace_back(std::byte(found));
		return data;
	}

	KeyOf_PackageLookupSideInput KeyOf_PackageLookupSideInput::deserialize(
		std::span<const std::byte> data
	) {
		constexpr std::string_view KEY_NAME = "KeyOf_PackageLookupSideInput";

		KeyOf_PackageLookupSideInput out;
		usize                        offset = 0;
		out.package_id                      = readStr(data, offset, KEY_NAME);
		out.found                           = readFlag(data, offset, KEY_NAME);
		return out;
	}

	void KeyOf_PackageLookupSideInput::prettyPrint(std::ostream& os) const {
		os << "KeyOf_PackageLookupSideInput {\n";
		os << "  package_id: " << package_id.strView() << "\n";
		os << "  found: " << (found ? "true" : "false") << "\n";
		os << "}\n";
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

	IMPLEMENT_QUERY_SIDE_INPUT(QueryPackageNameSideInput);

	IMPLEMENT_QUERY_SIDE_INPUT_WITH_LOGIC(QueryPackageLookupSideInput, ctx, key, {
		ctx.addMetadataIfNotExists<metadata_PackageLookup>(key);
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

	base::StrID PackageNameAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QueryPackageNameSideInput>(
			KeyOf_PackageNameSideInput::computeHash(m_package_id, m_name)
		);
		return m_name;
	}

	base::Optional<PackageAccessLocked> PackageLookupAccessLocked::unlock(query::Context& ctx
	) const {
		ctx.query<QueryPackageLookupSideInput>(KeyOf_PackageLookupSideInput{
			.package_id = m_package_id,
			.found      = m_found,
		});
		return illegalAccess();
	}

	base::Optional<PackageAccessLocked> PackageLookupAccessLocked::illegalAccess() const {
		if (!m_found) return {};
		return PackageAccessLocked(m_package_id);
	}

	namespace {
		/**
		 * @brief Get metadata of type @p Metadata from either the previous or the current graph,
		 * keeping only entries attached to nodes of @p SideInput.
		 */
		template<typename Metadata, typename SideInput>
		auto lookupsOf(bool from_previous_metadata) {
			auto all = from_previous_metadata
			             ? query::external::getMetadataFromAllPrevNodes<Metadata>()
			             : query::external::getMetadataFromAllCurrentNodes<Metadata>();
			// Metadata may be attached to other query types as well.
			std::erase_if(all, [](const auto& lookup) {
				return lookup.input_data.q_id != SideInput::getID();
			});
			return all;
		}
	}

	void collectPackageInputData(
		const std::vector<PackageInfo>&          packages,
		bool                                     from_previous_metadata,
		std::vector<query::external::InputData>& out
	) {
		base::StableHashMap<hashing::ComponentHash::HashType, const PackageInfo*> by_hash;
		std::unordered_set<base::StrID>                                           ids;

		for (const auto& package: packages) {
			by_hash.put(package.getPackageHash(), &package);
			ids.insert(package.getPackageID());

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
			out.emplace_back(
				QueryPackageNameSideInput::getID(),
				KeyOf_PackageNameSideInput::computeHash(
					package.getPackageID(), package.getName().illegalAccess()
				)
					.queryStablePerfectHash()
			);
		}

		for (const auto& lookup:
		     lookupsOf<metadata_PackageDependencyAliasLookup, QueryPackageDependencyAliasSideInput>(
				 from_previous_metadata
			 )) {
			const auto& key   = lookup.value->value;
			auto        owner = by_hash.atMaybe(key.package_hash);
			// Owner package no longer exists -> the lookup node stays red.
			if (not owner.has_value()) continue;

			auto target = (*owner.value())->getPackageDependencyByAlias(key.alias).illegalAccess();
			const bool found = target.has_value();
			if (found != key.found) continue;
			if (found && target.value().illegalAccess().getID() != key.target_package_id.value())
				continue;

			// Lookup result is unchanged -> reuse the stored InputData.
			out.emplace_back(lookup.input_data);
		}

		for (const auto& lookup:
		     lookupsOf<metadata_PackageLookup, QueryPackageLookupSideInput>(from_previous_metadata
		     )) {
			// Lookup result is unchanged -> reuse the stored InputData.
			if (ids.contains(lookup.value->value.package_id) == lookup.value->value.found)
				out.emplace_back(lookup.input_data);
		}
	}

}  // namespace compiler::frontend::packages
