#pragma once

#include <ser/errc.hpp>

#include <concepts>
#include <type_traits>
#include <utility>

namespace ser::internal {

	// the shape of every hook question
	// A hook can live in three places (the order is on ser::Serializer in serializer.hpp), and
	// each place is asked the same questions about the same four forms. Only the call
	// differs, so each place is a tag struct with one probe per form - access::trait_hooks
	// and access::member_hooks in access.hpp, AdlHooks in internal/adl.hpp - and every
	// question below is written once and asked with the tag passed as `L`.
	//
	// A probe is declared, never defined, and has a trailing return type, so a missing hook
	// is a substitution failure these requires-expressions can see.
	//
	// For `visit`, T keeps the object's const, so a hook that only takes a non-const object is
	// invisible on the write side; internal::checkHooks reports that.
	//
	// Each form comes in two versions. The loose one does not check the return type, so
	// "loose but not strict" means "the hook is there and returns the wrong thing", which
	// gets its own message.

	/** @brief The symmetric form, at level L: one hook for both directions. */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_VISIT_V = requires(Ar& ar, T& x) {
		{ L::visit(ar, x) } -> ::std::same_as<Errc>;
	};

	/** @brief A visit hook at level L, whatever it returns. */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_VISIT_LOOSE_V = requires(Ar& ar, T& x) { L::visit(ar, x); };

	/** @brief Write, at level L: the object exists and is const. */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_WRITE_V = requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
		{ L::write(ar, x) } -> ::std::same_as<Errc>;
	};

	/** @brief A write hook at level L, whatever it returns. */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_WRITE_LOOSE_V
		= requires(Ar& ar, const ::std::remove_cvref_t<T>& x) { L::write(ar, x); };

	/** @brief Read, at level L: the object exists and is filled in place. */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_READ_V = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
		{ L::read(ar, x) } -> ::std::same_as<Errc>;
	};

	/** @brief A read hook at level L, whatever it returns. */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_READ_LOOSE_V
		= requires(Ar& ar, ::std::remove_cvref_t<T>& x) { L::read(ar, x); };

	/** @brief Make, at level L: there is no object yet, so the hook returns one by value. */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_MAKE_V = requires(Ar& ar) {
		{ L::template make<T>(ar) } -> ::std::same_as<::std::remove_cvref_t<T>>;
	};

	/** @brief A make hook at level L, whatever it returns. */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_MAKE_LOOSE_V = requires(Ar& ar) { L::template make<T>(ar); };

	/**
	 * @brief The read hook at level L accepts an RVALUE object.
	 *
	 * The discriminator for a read hook that takes its object BY VALUE - one missing `&`. A
	 * `T&` parameter refuses an rvalue and so does a deduced `auto&`, which is the generic
	 * spelling of a correct hook; a by-value or `const&` parameter accepts one. So
	 * "HAS_READ_V says yes AND this says yes" means the hook is being called and is reading
	 * into a copy that is thrown away. Every `read` probe forwards, which is what lets the
	 * same probe answer both questions.
	 *
	 * Only the read form can be asked this. A visit hook has to accept a const object -
	 * that is the write direction, and SER_INTERNAL_ASSERT_VISIT_TAKES_CONST demands it - so
	 * an rvalue binds to the const overload and a by-value serVisit is indistinguishable
	 * from a correct one. serRead has no such overload: reading into a const object means
	 * nothing.
	 */
	template<class L, class T, class Ar>
	inline constexpr bool HAS_READ_RVALUE_V
		= requires(Ar& ar, ::std::remove_cvref_t<T>&& x) { L::read(ar, ::std::move(x)); };

}  // namespace ser::internal
