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

	/// One top-level constant as its own input: the key carries the constant's
	/// identity (qualified-name hash) and value (+type) hash. Functions record a
	/// dependency at the use site, so a const edit invalidates only its readers —
	/// where psacklinger's coarse globals Mod recompiles everything.
	struct ConstInputKey {
		u64 session_epoch;
		u64 name_hash;
		u64 value_hash;

		bool operator==(const ConstInputKey&) const = default;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
			return { session_epoch, name_hash, value_hash, 3 };
		}
	};

	DECLARE_QUERY_SIDE_INPUT(ConstInput, ConstInputKey);

	/// One function's signature (link name + parameter/return types) as an input.
	/// Callers depend on it through getOrCreateFunction, so a signature change
	/// invalidates exactly the function and its callers.
	struct SigInputKey {
		u64 session_epoch;
		u64 name_hash;
		u64 value_hash;

		bool operator==(const SigInputKey&) const = default;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
			return { session_epoch, name_hash, value_hash, 4 };
		}
	};

	DECLARE_QUERY_SIDE_INPUT(SigInput, SigInputKey);

	/// One class's layout (field names + types) as an input; users depend on it
	/// through struct type lowering.
	struct ClassInputKey {
		u64 session_epoch;
		u64 name_hash;
		u64 value_hash;

		bool operator==(const ClassInputKey&) const = default;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
			return { session_epoch, name_hash, value_hash, 5 };
		}
	};

	DECLARE_QUERY_SIDE_INPUT(ClassInput, ClassInputKey);

	/// One global variable's identity + type as an input; readers depend on it
	/// through getOrCreateGlobal. Initializers are covered by GlobalDefsInput —
	/// only the globals object cares about those.
	struct GlobalVarInputKey {
		u64 session_epoch;
		u64 name_hash;
		u64 value_hash;

		bool operator==(const GlobalVarInputKey&) const = default;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
			return { session_epoch, name_hash, value_hash, 6 };
		}
	};

	DECLARE_QUERY_SIDE_INPUT(GlobalVarInput, GlobalVarInputKey);

	/// Content hash of all global variable declarations, initializers included;
	/// the globals-object emission depends on it.
	struct GlobalDefsInputKey {
		u64 session_epoch;
		u64 defs_hash;

		bool operator==(const GlobalDefsInputKey&) const = default;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
			return { session_epoch, defs_hash, 0, 7 };
		}
	};

	DECLARE_QUERY_SIDE_INPUT(GlobalDefsInput, GlobalDefsInputKey);

	/// Coarse structural side input: imports, namespace names and module identity.
	/// Everything else is tracked through the per-symbol inputs above.
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
