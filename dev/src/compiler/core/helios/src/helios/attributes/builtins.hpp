#pragma once

#include <frontend/pst_parser/access.hpp>
#include <helios/hout/hout_fd.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/flag.hpp>

#include <string_id/string_id.hpp>

namespace query {
	struct Context;
}

namespace pst {
	class AtrArgList;
}

/**
 * This decides what layer implements the builtin. For example `ptr_from_slice`
 * is implemented in HOUT, but in the future some builtins will be implemented only
 * in DVM Backend or LLVM backend.
 */
MAKE_FLAG_TYPE(compiler::helios, BuiltinOrigin, BuiltinOrigins, HOUT, DVMBackend, NativeBackend);

namespace compiler::helios {
	struct SymID;

	/**
	 * @brief Identifies which compiler builtin a `@builtin("...")` attribute selects.
	 * Can't use the STRINGIFIYABLE enum because camel case vs snake case.
	 */
	enum class BuiltinKind {
		CharPtrFromSlice,
		CharSliceFromPtrLen,
		DvmCharAlloc,
		DvmCharRealloc,
		DvmCharFree
	};

	/**
	 * @brief Reverse of builtinKindFromStr.
	 */
	base::StrID builtinKindToStr(BuiltinKind type);

	/**
	 * @brief Origin for the given builtin.
	 */
	BuiltinOrigins getBuiltinOrigins(BuiltinKind type);

	/**
	 * @brief Validate the arguments of a `@builtin(...)` attribute and resolve the builtin.
	 *
	 * Requires exactly one string-literal argument naming a known builtin. On any violation it logs
	 * a diagnostic pointing at the offending argument and fails the current query (never returns).
	 *
	 * @param args The attribute argument list, empty when the attribute is written without `(...)`.
	 */
	BuiltinKind parseBuiltinAttr(
		query::Context& ctx, base::Optional<pst::AccessLocked<pst::AtrArgList>> args
	);

	/**
	 * @brief Build the HOUT implementation of a builtin function.
	 * @param symbol The builtin's function symbol (used to fetch its declaration/parameters).
	 * @param type   Which builtin to implement.
	 */
	HOUTFunction getBuiltinImpl(query::Context& ctx, SymID symbol, BuiltinKind type);
}
