#include "access.hpp"

#include "functors.hpp"
#include "module_tree.hpp"
#include "queries.hpp"
#include "source_file.hpp"

#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::frontend {

	IMPLEMENT_QUERY_SIDE_INPUT(QueryModuleSideInput);
	IMPLEMENT_QUERY_SIDE_INPUT(QueryFileSideInput);
	IMPLEMENT_QUERY_SIDE_INPUT(QuerySubmoduleCountSideInput);

	IMPLEMENT_QUERY_SIDE_INPUT_WITH_LOGIC(QueryModuleChildSideInput, ctx, key, {
		ctx.addMetadataIfNotExists<metadata_ModuleLookup>(key);
	});


	SubmodulesAccessLocked::SubmodulesAccessLocked(
		ModuleID module, std::vector<ModuleAccessLocked> submodules
	):
		  module(module),
		  submodules(std::move(submodules)) {}

	std::vector<ModuleAccessLocked> SubmodulesAccessLocked::illegalAccess() const {
		return submodules;
	}

	std::vector<ModuleAccessLocked> SubmodulesAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QuerySubmoduleCountSideInput>(
			KeyOf_SubmoduleCountSideInput::computeHash(module, submodules.size())
		);
		return submodules;
	}

	query::QueryStableHash KeyOf_ModuleSideInput::queryStablePerfectHash() const {
		return stable_hash;
	}

	query::QueryStableHash KeyOf_FileSideInput::queryStablePerfectHash() const {
		return stable_hash;
	}

	template<>
	ModuleAccess ModuleAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QueryModuleSideInput>(KeyOf_ModuleSideInput{ ModuleTree::getModuleHash(id) });
		return ModuleAccess(id);
	}

	template<>
	FileAccess FileAccessLocked::unlock(query::Context& ctx) const {
		// We can use file component hash for stable hash of FileID
		// This is because we can only get from file 1) Its parent module 2) Its name
		// Both are included in component hash
		// The PST Tree has its own access side input for its content
		// Also: Every SourceFile has a PST associated with it
		// This might change in the future but for now we don't need much from a SourceFile itself
		ctx.query<QueryFileSideInput>(KeyOf_FileSideInput{ getFileRef(id)->getComponentHash().hash }
		);
		return FileAccess(id);
	}

	query::QueryStableHash KeyOf_SourceFileCountSideInput::queryStablePerfectHash() const {
		return stable_hash;
	}

	KeyOf_SourceFileCountSideInput KeyOf_SourceFileCountSideInput::computeHash(
		ModuleID module_id, usize count
	) {
		auto hasher = ModuleTree::getPathComponentHash(module_id).partial;
		hashing::addToHash(hasher, static_cast<u64>(count));
		return KeyOf_SourceFileCountSideInput{ hasher.finalize() };
	}

	query::QueryStableHash KeyOf_SubmoduleCountSideInput::queryStablePerfectHash() const {
		return stable_hash;
	}

	KeyOf_SubmoduleCountSideInput KeyOf_SubmoduleCountSideInput::computeHash(
		ModuleID module_id, usize count
	) {
		auto hasher = ModuleTree::getPathComponentHash(module_id).partial;
		hashing::addToHash(hasher, static_cast<u64>(count));
		return KeyOf_SubmoduleCountSideInput{ hasher.finalize() };
	}

	query::QueryStableHash KeyOf_ModuleChildSideInput::queryStablePerfectHash() const {
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, parent_hash);
		hashing::addToHash(hasher, child_name);
		hashing::addToHash(hasher, found);
		return hasher.finalize();
	}

	std::vector<std::byte> KeyOf_ModuleChildSideInput::serialize() const {
		std::vector<std::byte> data;
		auto                   hash_ptr = reinterpret_cast<const std::byte*>(&parent_hash);
		data.insert(data.end(), hash_ptr, hash_ptr + sizeof(hashing::ComponentHash::HashType));
		auto size     = child_name.size();
		auto size_ptr = reinterpret_cast<const std::byte*>(&size);
		data.insert(data.end(), size_ptr, size_ptr + sizeof(size));
		auto searched_ptr = reinterpret_cast<const std::byte*>(child_name.data());
		data.insert(data.end(), searched_ptr, searched_ptr + child_name.size());
		data.emplace_back(std::byte(found));
		return data;
	}

	KeyOf_ModuleChildSideInput KeyOf_ModuleChildSideInput::deserialize(std::span<const std::byte> data
	) {
		constexpr usize MIN_SIZE = sizeof(hashing::ComponentHash::HashType) + sizeof(usize);
		CORE_ASSERT(
			data.size() >= MIN_SIZE,
			"KeyOf_ModuleChildSideInput::deserialize: data buffer too small for header"
		);

		KeyOf_ModuleChildSideInput lookup_data;
		usize                      offset = 0;

		// @TODO: #1942 fix this
		// ...
		// It would be better to have this be done in more controlled manner,
		// such that, if the HashType changes it will still work or produce a compilation error.
		// I would assume this could just call some kind of deser::deserialize(...)
		// which would propagate the call to appropriate case or give a compilation error that the
		// given type is not deserializable. This applies in more general way to this whole function.

		std::memcpy(&lookup_data.parent_hash, data.data(), sizeof(hashing::ComponentHash::HashType));
		offset += sizeof(hashing::ComponentHash::HashType);
		usize len = 0;
		std::memcpy(&len, data.data() + offset, sizeof(usize));
		offset += sizeof(usize);

		CORE_ASSERT(
			data.size() >= offset + len + sizeof(std::byte),
			"KeyOf_ModuleChildSideInput::deserialize: data buffer",
			" too small for child_name and found flag"
		);

		auto* ptr = reinterpret_cast<const char*>(data.data() + offset);
		offset += len;
		lookup_data.child_name = base::StrID({ ptr, len });  // this makes a copy
		auto was_found         = std::byte(0);
		std::memcpy(&was_found, data.data() + offset, sizeof(std::byte));
		lookup_data.found = bool(was_found);
		return lookup_data;
	}

	void KeyOf_ModuleChildSideInput::prettyPrint(std::ostream& os) const {
		os << "KeyOf_ModuleChildSideInput {\n";
		os << "  parent_hash: " << parent_hash.toStringHex() << "\n";
		os << "  child_name: " << child_name.strView() << "\n";
		os << "  found: " << (found ? "true" : "false") << "\n";
		os << "}\n";
	}

	base::Optional<ModuleAccessLocked> ModuleChildAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QueryModuleChildSideInput>(KeyOf_ModuleChildSideInput{
			.parent_hash = ModuleTree::getModuleHash(parent),
			.child_name  = child_name,
			.found       = child_id.has_value() });
		if (!child_id.has_value()) return {};
		return ModuleAccessLocked(child_id.value());
	}

	base::Optional<ModuleAccessLocked> ModuleChildAccessLocked::illegalAccess() const {
		if (!child_id.has_value()) return {};
		return ModuleAccessLocked(child_id.value());
	}
}
