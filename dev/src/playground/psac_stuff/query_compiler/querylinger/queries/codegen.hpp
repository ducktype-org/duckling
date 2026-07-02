#pragma once

#include <base/types/ints.hpp>

#include <query_framework/query_int.hpp>

#include <string>

namespace ql {
	/// Result of one object-emission query. Compilation problems are carried as a
	/// message instead of an exception so they don't unwind into the task pool
	/// (the session rethrows after awaiting all results).
	struct ObjectResult {
		std::string error;
	};

	/// Emits one function's object file (via the shared psackling lowering). Keyed by
	/// the function's link name so the node survives body edits; the provider records
	/// dependencies on the FnBodyInput and GlobalsInput side inputs, which is what
	/// invalidation propagates through.
	struct FnObjectKey {
		u64         session_epoch;
		u64         link_hash;
		std::string link_name;

		bool operator==(const FnObjectKey&) const = default;

		[[nodiscard]] u64 queryUnstablePerfectHash() const {
			return link_hash * 0x9E'37'79'B9'7F'4A'7C'15ULL + session_epoch;
		}
	};

	DECLARE_QUERY(QueryFunctionObject, FnObjectKey, ObjectResult, ({ .uses_qresult = false }));

	/// Emits the globals object file (global variable definitions, builtins and the
	/// synthetic initializer constructor). Depends on the GlobalsInput side input.
	struct GlobalsObjectKey {
		u64 session_epoch;

		bool operator==(const GlobalsObjectKey&) const = default;

		[[nodiscard]] u64 queryUnstablePerfectHash() const { return session_epoch; }
	};

	DECLARE_QUERY(QueryGlobalsObject, GlobalsObjectKey, ObjectResult, ({ .uses_qresult = false }));

	/// How many function-object (resp. globals-object) providers actually ran since
	/// the last reset; the session uses them to report recompile counts like
	/// psacklinger's CompileStats and to decide whether a relink is needed.
	usize functionRecompileCount();
	usize globalsObjectRecompileCount();
	void  resetRecompileCounts();
}
