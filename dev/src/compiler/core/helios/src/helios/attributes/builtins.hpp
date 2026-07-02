#pragma once

#include <frontend/pst_parser/access.hpp>

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

namespace query {
	struct Context;
}

namespace pst {
	class AtrArgList;
}

namespace compiler::helios {
	/**
	 * @brief Identifies which compiler builtin a `@builtin("...")` attribute selects.
	 */
	enum class BuiltinType {
		RawPtrFromSlice,
	};

	/**
	 * @brief Map a builtin name to its BuiltinType, empty when the name is unknown.
	 */
	base::Optional<BuiltinType> builtinTypeFromStr(base::StrID name);

	/**
	 * @brief Reverse of builtinTypeFromStr.
	 */
	base::StrID builtinTypeToStr(BuiltinType type);

	/**
	 * @brief Validate the arguments of a `@builtin(...)` attribute and resolve the builtin.
	 *
	 * Requires exactly one string-literal argument naming a known builtin. On any violation it logs
	 * a diagnostic pointing at the offending argument and fails the current query (never returns).
	 *
	 * @param args The attribute argument list, empty when the attribute is written without `(...)`.
	 */
	BuiltinType parseBuiltinAttr(
		query::Context& ctx, base::Optional<pst::AccessLocked<pst::AtrArgList>> args
	);
}
