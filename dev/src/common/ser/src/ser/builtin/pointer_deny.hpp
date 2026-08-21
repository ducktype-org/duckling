#pragma once

#include <ser/concepts.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>

#include <type_traits>

namespace ser::builtin {

	// std::reference_wrapper without dragging in <functional>: unwrap_reference_t<T> is
	// T& exactly when T is a reference_wrapper, and it lives in <type_traits>.
	template<class T>
	inline constexpr bool REFERENCE_WRAPPER_LIKE_V
		= !::std::is_same_v<::std::unwrap_reference_t<T>, T>;

	template<class T>
	inline constexpr bool CHAR_LIKE_V
		= ::std::is_same_v<T, char> || ::std::is_same_v<T, signed char>
	   || ::std::is_same_v<T, unsigned char> || ::std::is_same_v<T, wchar_t>
	   || ::std::is_same_v<T, char8_t> || ::std::is_same_v<T, char16_t>
	   || ::std::is_same_v<T, char32_t>;

	// Refused outright, in every dispatch context. Not "unsupported for now" - each of
	// these has no meaning outside the writing process, and guessing one would be a
	// silent data-corruption bug rather than a compile error.
	template<class T>
	inline constexpr bool DENIED_V
		= ::std::is_pointer_v<T> || ::std::is_reference_v<T> || ::std::is_member_pointer_v<T>
	   || ::std::is_union_v<T> || REFERENCE_WRAPPER_LIKE_V<T>;

	// Fires the one message that fits. These strings are the entire user interface of a
	// refusal, so each names the replacement rather than just the problem.
	template<class T>
	constexpr void deny() {
		using P = ::std::remove_cv_t<::std::remove_pointer_t<T>>;

		if constexpr (::std::is_pointer_v<T> && CHAR_LIKE_V<P>)
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: cannot serialize a raw string pointer - the length is not part of "
				"the type and the bytes are not owned.\n"
				"  Owning text?      std::string (block E, <ser/std/string.hpp>)\n"
				"  Interned handle?  base::StrID (M2)\n"
				"  Fixed buffer?     char[N] - a C array of known extent serializes as itself"
			);

		else if constexpr (::std::is_pointer_v<T>
		                   && ::std::is_function_v<::std::remove_pointer_t<T>>)
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: cannot serialize a function pointer - a code address means nothing in "
				"another process, and nothing in the stream could validate it. Write a tag "
				"(an enum, or an index into a table you own) and map it back after reading."
			);

		else if constexpr (::std::is_pointer_v<T> && ::std::is_void_v<P>)
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: cannot serialize void* - neither the size nor the type of what it "
				"points at is knowable. Serialize the object itself, or std::span<const "
				"std::byte> through an explicit byte-buffer field."
			);

		else if constexpr (::std::is_pointer_v<T>)
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: cannot serialize a raw pointer - an address means nothing in another "
				"process, and nothing in the type says whether it owns what it points at.\n"
				"  Owns the object?     base::Box<T> (M3)\n"
				"  Points at one?       base::Ref<T> (M3)\n"
				"  May be absent?       std::optional<T> (block E)\n"
				"  Index into a table?  store the index as a plain integer"
			);

		else if constexpr (::std::is_member_pointer_v<T>)
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: cannot serialize a pointer to member - it is an offset into a layout "
				"that the reading program is not required to share. Write a tag or an index "
				"and select the member yourself after reading."
			);

		else if constexpr (::std::is_reference_v<T>)
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: cannot serialize a reference - reading has to overwrite the object, "
				"and a reference can never be rebound. Serialize the referenced value, or "
				"store an index into the container that owns it."
			);

		else if constexpr (REFERENCE_WRAPPER_LIKE_V<T>)
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: cannot serialize std::reference_wrapper - it is a rebindable "
				"reference, and the object it names does not travel with it. Serialize the "
				"referenced value, or store an index into the container that owns it."
			);

		else if constexpr (::std::is_union_v<T>)
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: cannot serialize a union - nothing in the bytes says which member is "
				"alive, so reading one back would be undefined behaviour rather than a "
				"wrong value. Use std::variant (M2), or write a tag next to the payload and "
				"a serVisit that switches on it."
			);

		else
			static_assert(
				detail::DEPENDENT_FALSE<T>,
				"ser: internal error - deny() reached for a type that DENIED_V does not "
				"refuse. Report this."
			);
	}

	// ── the unregistered smart pointer ────────────────────────────────────────
	// A heuristic, and used only as one: it runs at the very bottom of the ladder, where
	// the answer is already "no way to serialize this" and the only question left is
	// which sentence to print.
	template<class T>
	consteval bool looksPointerLike() {
		return requires(T& t) { *t; } || requires(T& t) { t.operator->(); };
	}

	template<class T>
	constexpr void denyPointerLike() {
		static_assert(
			detail::DEPENDENT_FALSE<T>,
			"ser: this type has operator* / operator-> and no registered serializer, so it "
			"looks like a smart pointer or a handle.\n"
			"  std::optional?          #include <ser/std/optional.hpp>\n"
			"  unique_ptr/shared_ptr?  not supported yet\n"
			"  base::Box / base::Ref?  not supported yet\n"
			"  your own handle type?   specialize ser::serializer<T>"
		);
	}

}  // namespace ser::builtin
