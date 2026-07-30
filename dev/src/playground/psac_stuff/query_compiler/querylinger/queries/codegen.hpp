#pragma once

#include <base/types/bit256.hpp>

#include <hashing/hash.hpp>
#include <query_framework/query_int.hpp>

#include <string>

namespace ql {
	/// Result of one object-emission query: the object file bytes. The providers are
	/// pure — they never touch the filesystem; the session decides where (and
	/// whether) the bytes are written. Compilation problems are carried as a message
	/// instead of an exception so they don't unwind into the task pool (the session
	/// rethrows after awaiting all results).
	struct ObjectResult {
		std::string error;
		std::string object_bytes;
	};

	/// Emits one function's object bytes (via the shared psackling lowering). Keyed
	/// by the function's link name so the node survives body edits; the provider
	/// records dependencies on the FnBodyInput, GlobalsInput and per-symbol side
	/// inputs, which is what invalidation propagates through.
	struct FnObjectKey {
		std::string link_name;

		bool operator==(const FnObjectKey&) const = default;

		[[nodiscard]] base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(link_name);
		}
	};

	DECLARE_QUERY(QueryFunctionObject, FnObjectKey, ObjectResult, ({ .uses_qresult = false }));

	/// Emits the globals object bytes (global variable definitions, builtins and the
	/// synthetic initializer constructor). A singleton per program: its identity is
	/// carried entirely by the GlobalsInput and GlobalDefsInput dependencies.
	struct GlobalsObjectKey {
		bool operator==(const GlobalsObjectKey&) const = default;

		[[nodiscard]] u64 queryUnstablePerfectHash() const { return 0; }
	};

	DECLARE_QUERY(QueryGlobalsObject, GlobalsObjectKey, ObjectResult, ({ .uses_qresult = false }));
}
