#pragma once

#include <base/types/ints.hpp>

#include <query_framework/input_query/query_input.hpp>

namespace ql {
	/// Side input standing for one function's body content: the key hash IS the input
	/// value (the framework's analog of psacklinger's per-function psac::Mod), so a
	/// body edit produces a different input node and invalidates its dependents.
	/// The session epoch keeps nodes of unrelated compiler sessions apart.
	struct FnBodyInputKey {
		u64 session_epoch;
		u64 link_hash;
		u64 body_hash;

		bool operator==(const FnBodyInputKey&) const = default;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
			return { session_epoch, link_hash, body_hash, 1 };
		}
	};

	DECLARE_QUERY_SIDE_INPUT(FnBodyInput, FnBodyInputKey);

	/// Coarse "everything global" side input — psacklinger's globals Mod as a query
	/// node: classes, consts, global variables, imports and function signatures all
	/// fold into one hash. Split into per-symbol inputs in later milestones.
	struct GlobalsInputKey {
		u64 session_epoch;
		u64 globals_hash;

		bool operator==(const GlobalsInputKey&) const = default;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
			return { session_epoch, globals_hash, 0, 2 };
		}
	};

	DECLARE_QUERY_SIDE_INPUT(GlobalsInput, GlobalsInputKey);
}
