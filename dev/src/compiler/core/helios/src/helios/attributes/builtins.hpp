#pragma once

#include <frontend/pst_parser/access.hpp>
#include <helios/hout/hout_fd.hpp>
#include <helios/hout/origin.hpp>
#include <helios/tsh/abstract_type.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/flag.hpp>

#include <string_id/string_id.hpp>

namespace query {
	struct Context;
}

namespace pst {
	class AtrArgList;
}

namespace compiler::helios::code {
	struct Expr;
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
		DvmCharFree,
		/** `size_of(v: meta) -> i64`: byte size of a type. Implemented in HOUT as a `SizeOf` op. */
		SizeOf,
		/** `alignment_of(v: meta) -> i64`: byte alignment of a type. HOUT `AlignOf` op. */
		AlignmentOf,
		/**
		 * Box allocation / deallocation, dynamic-array (list) freeing and the box destructor.
		 * Unlike the other builtins these are not selected by the `@builtin("...")` attribute. They
		 * are only called by the compiler in `box T`/`[T]` constructors and destructors.
		 *
		 * `BoxAlloc`/`BoxFree`/`ListFree` are implemented by the backends; `BoxDestructor` is
		 * implemented in HOUT (it destroys the pointee, then calls `box_free`).
		 */
		BoxAlloc,
		BoxFree,
		ListFree,
		BoxDestructor,
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

	/**
	 * @brief Symbol of the compiler-generated `box_alloc(value: T) -> box T` builtin for a given
	 * pointee type.
	 *
	 * The returned symbol is a declaration only, it's implemented in both backends.
	 */
	SymID boxAllocSymForType(query::Context& ctx, tsh::AbstractType pointee_type);

	/**
	 * @brief Symbol of the compiler-generated `box_free(b: box T)` builtin for a given pointee type.
	 *
	 * The returned symbol is a declaration only, it's implemented in both backends.
	 */
	SymID boxFreeSymForType(query::Context& ctx, tsh::AbstractType pointee_type);


	/**
	 * @brief Symbol of the compiler-generated `box_destructor(b: box T)` builtin for a given
	 * pointee type.
	 *
	 * Unlike `box_free`, this is implemented in HOUT: it destroys the pointee first, then frees the
	 * box storage via `box_free`. It is the destructor used for `box T` values.
	 */
	SymID boxDestructorSymForType(query::Context& ctx, tsh::AbstractType pointee_type);

	/**
	 * @brief Symbol of the compiler-generated `list_free(l: ref List[T])` builtin for a given element
	 * type.
	 *
	 * The returned symbol is a declaration only, it's implemented in both backends.
	 */
	SymID listFreeSymForType(query::Context& ctx, tsh::AbstractType element_type);

	/**
	 * @brief Build a HOUT expression that constructs a `box T` holding `inner`.
	 */
	Box<code::Expr> makeBoxAllocCall(
		query::Context& ctx, code::ElementOrigin origin, Box<code::Expr> inner
	);
}
