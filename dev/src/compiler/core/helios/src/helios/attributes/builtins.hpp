#pragma once

#include <frontend/pst_parser/access.hpp>
#include <helios/hout/hout_fd.hpp>

#include <base/extend_cpp/flag.hpp>
#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

namespace query {
	struct Context;
}

namespace pst {
	class AtrArgList;
}

/**
 * This decided what layer implements the builtin. For example `ptr_from_slice`
 * is implemented in HOUT, but in the future some builtins will be implemented only
 * in DVM Backend or LLVM backend.
 */
MAKE_FLAG_TYPE(compiler::helios, BuiltinOrigin, BuiltinOrigins, HOUT, DVMBackend, NativeBackend);

namespace compiler::helios {
	struct SymID;

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
	 * @brief Origin for the given builtin.
	 */
	BuiltinOrigins builtinOriginForType(BuiltinType type);

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

	/**
	 * @brief Build the HOUT implementation of a builtin function.
	 * @param symbol The builtin's function symbol (used to fetch its declaration/parameters).
	 * @param type   Which builtin to implement.
	 */
	HOUTFunction getBuiltinImpl(query::Context& ctx, SymID symbol, BuiltinType type);
}
