#pragma once

#include "file_id.hpp"
#include "module_id.hpp"

#include <base/collections/optional.hpp>

#include <query_framework/utils/query_hash.hpp>

namespace compiler::frontend {

	struct KeyOf_ModuleSideInput {
		ModuleID id;

		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const;
		bool                   operator==(const KeyOf_ModuleSideInput&) const = default;
	};

	struct KeyOf_FileSideInput {
		FileID id;

		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const;
		bool                   operator==(const KeyOf_FileSideInput&) const = default;
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

}
