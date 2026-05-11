#pragma once

#include "file_id.hpp"
#include "module_id.hpp"

#include <base/collections/optional.hpp>

#include <hashing/component_hash.hpp>
#include <query_framework/utils/query_hash.hpp>
#include <string_id/string_id.hpp>

namespace compiler::frontend {

	/**
	 * @brief Key for module side input query.
	 * It stores the hash itself for the performance reasons
	 * For more info see QueryModuleSideInput query.
	 */
	struct KeyOf_ModuleSideInput final {
		query::QueryStableHash stable_hash;

		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const;
		bool                   operator==(const KeyOf_ModuleSideInput&) const = default;
	};

	/**
	 * @brief Key for file side input query.
	 * It stores the hash itself for the performance reasons
	 * For more info see QueryFileSideInput query.
	 */
	struct KeyOf_FileSideInput final {
		query::QueryStableHash stable_hash;

		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const;
		bool                   operator==(const KeyOf_FileSideInput&) const = default;
	};

	/**
	 * @brief Key for submodule count side input query.
	 * It stores the hash itself for the performance reasons
	 * For more info see QuerySubmoduleCountSideInput query.
	 */
	struct KeyOf_SubmoduleCountSideInput final {
		query::QueryStableHash stable_hash;

		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const;
		bool                   operator==(const KeyOf_SubmoduleCountSideInput&) const = default;

		/**
		 * @brief Compute the stable hash for submodule count side input.
		 * @param module_id The module whose submodules are being counted.
		 * @param count Number of submodules in the module.
		 * @return KeyOf_SubmoduleCountSideInput with computed hash.
		 */
		[[nodiscard]] static KeyOf_SubmoduleCountSideInput computeHash(
			ModuleID module_id, usize count
		);
	};

	/**
	 * @brief Key for module child side input query.
	 * It stores the parent module hash, child name and whether the child was found.
	 * This is needed to be in the key to store the metadata, and recreate this input during driver
	 * initialization. For more info see QueryModuleChildSideInput query.
	 * @note This key is used as MetadataType and it's stored in metadata during the provide call.
	 * That's why it implements the serialize/deserialize methods.
	 * These methods are called during metadata serialization/deserialization.
	 */
	struct KeyOf_ModuleChildSideInput final {
		hashing::ComponentHash::HashType parent_hash;  // hash of the parent module
		base::StrID                      child_name;   // name of the child module
		/// @brief Whether the child module exists in the parent module.
		/// This field is part of the key so that dependencies can distinguish between successful
		/// lookups (found = true) and failed lookups (found = false) for the same child name.
		bool found;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const;
		bool operator==(const KeyOf_ModuleChildSideInput&) const = default;

		[[nodiscard]] std::vector<std::byte> serialize() const;
		static KeyOf_ModuleChildSideInput    deserialize(std::span<const std::byte> data);

		void prettyPrint(std::ostream& os) const;
	};

	/**
	 * @brief Wrapper for  (ModuleID or FileID) that requires unlocking via Context to access.
	 * Unlocking registers a dependency (SideInput).
	 */
	template<typename IDType>
	class AccessLocked;

	/**
	 * @brief Unlocked access to a resource (ModuleID or FileID).
	 * Provides access to the ID which can be used for further queries.
	 */
	template<typename IDType>
	class Access final {
		IDType id;
		friend class AccessLocked<IDType>;

		explicit Access(IDType id): id(id) {}

	public:
		[[nodiscard]] IDType getID() const { return id; }
	};

	/**
	 * @brief Wrapper for an ID that requires unlocking via Context to access.
	 * Unlocking registers a dependency on the resource (SideInput).
	 */
	template<typename IDType>
	class AccessLocked final {
		IDType id;

	public:
		explicit AccessLocked(IDType id): id(id) {}

		Access<IDType> unlock(query::Context& ctx) const;

		/**
		 * @brief Access that doesn't require passing context. It should only be used outside of
		 * queries unless for debuging purposes
		 */
		[[nodiscard]] Access<IDType> illegalAccess() const { return Access<IDType>(id); }

		// Allow moving
		AccessLocked(AccessLocked&&)                 = default;
		AccessLocked& operator=(AccessLocked&&)      = default;
		AccessLocked(const AccessLocked&)            = default;
		AccessLocked& operator=(const AccessLocked&) = default;
	};

	using ModuleAccess       = Access<ModuleID>;
	using ModuleAccessLocked = AccessLocked<ModuleID>;

	using FileAccess       = Access<FileID>;
	using FileAccessLocked = AccessLocked<FileID>;

	/**
	 * @brief This lock holds a vector of ModuleAccessLocked for each submodule.
	 * This is needed to register the dependency on the number of submodules in the module.
	 */
	class SubmodulesAccessLocked final {
		ModuleID                        module;
		std::vector<ModuleAccessLocked> submodules;

	public:
		SubmodulesAccessLocked(ModuleID module, std::vector<ModuleAccessLocked> submodules);

		[[nodiscard]] std::vector<ModuleAccessLocked> unlock(query::Context& ctx) const;
		[[nodiscard]] std::vector<ModuleAccessLocked> illegalAccess() const;
	};

	class ModuleChildAccessLocked final {
		ModuleID                 parent;
		base::StrID              child_name;
		base::Optional<ModuleID> child_id;

	public:
		explicit ModuleChildAccessLocked(
			ModuleID parent, base::StrID child_name, base::Optional<ModuleID> child_id
		):
			  parent(parent),
			  child_name(child_name),
			  child_id(child_id) {}

		[[nodiscard]]
		base::Optional<ModuleAccessLocked> unlock(query::Context& ctx) const;
		[[nodiscard]]
		base::Optional<ModuleAccessLocked> illegalAccess() const;
	};
}
