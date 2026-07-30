#pragma once

#include <base/types/bit256.hpp>

#include <hashing/hash.hpp>
#include <query_framework/input_query/query_input.hpp>

#include <string>

namespace ql {
	/// All side inputs share one shape: the key is a single SHA-256 hash that IS the
	/// input's identity and value at once (the framework's analog of psacklinger's
	/// psac::Mod, and the same shape as the compiler's PSTAccessKey). A content
	/// change produces a different input node, and diffing the input sets between
	/// compiles invalidates the dependents of the vanished nodes. Input categories
	/// are distinguished by their QueryID (part of the node identity), not by the
	/// hash itself.
	struct HashInputKey {
		base::Bit256 hash;

		bool operator==(const HashInputKey&) const = default;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const { return hash; }
	};

	/// One function's body content: SHA-256 over (link name, position-stripped body
	/// print), so a body edit invalidates exactly that function's emission.
	DECLARE_QUERY_SIDE_INPUT(FnBodyInput, HashInputKey);

	/// Folds a function's link name and body content hash into the FnBodyInput key.
	inline base::Bit256 fnBodyInputHash(const std::string& link_name, const base::Bit256& body_hash) {
		return hashing::justHash<hashing::SHA256>(std::string_view("fnbody"), link_name, body_hash);
	}

	/// One top-level constant (qualified name + value + type, see
	/// pg::constInputHash). Functions record the dependency at the use site, so a
	/// const edit invalidates only its readers — where psacklinger's coarse globals
	/// Mod recompiles everything.
	DECLARE_QUERY_SIDE_INPUT(ConstInput, HashInputKey);

	/// One function's signature (link name + parameter/return types, see
	/// pg::functionSigInputHash). Callers depend on it through getOrCreateFunction,
	/// so a signature change invalidates exactly the function and its callers.
	DECLARE_QUERY_SIDE_INPUT(SigInput, HashInputKey);

	/// One class's layout (qualified name + field names and types, see
	/// pg::classLayoutInputHash); users depend on it through struct type lowering.
	DECLARE_QUERY_SIDE_INPUT(ClassInput, HashInputKey);

	/// One global variable's identity + type (see pg::globalVarInputHash); readers
	/// depend on it through getOrCreateGlobal. Initializers are covered by
	/// GlobalDefsInput — only the globals object cares about those.
	DECLARE_QUERY_SIDE_INPUT(GlobalVarInput, HashInputKey);

	/// Content hash of all global variable declarations, initializers included;
	/// the globals-object emission depends on it.
	DECLARE_QUERY_SIDE_INPUT(GlobalDefsInput, HashInputKey);

	/// Coarse structural side input: imports, namespace names and module identity.
	/// Everything else is tracked through the per-symbol inputs above.
	DECLARE_QUERY_SIDE_INPUT(GlobalsInput, HashInputKey);
}
