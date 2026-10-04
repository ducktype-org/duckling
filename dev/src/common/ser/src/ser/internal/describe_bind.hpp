#pragma once

#include <base/comptime/aggregate_arity.hpp>
#include <base/comptime/member_walk.hpp>
#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>

#include <ser/access.hpp>
#include <ser/config.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser::internal {

	/** @brief Taken from the ladder itself, so the two cannot drift apart. */
	inline constexpr ::std::size_t MAX_MEMBERS = ::base::LADDER_MAX;

	/**
	 * @brief "Braces here do not mean fields." For SER_MAKE_FROM, which expands to braces and
	 * has to refuse a type whose braces reach an initializer_list constructor instead.
	 */
	template<class T>
	inline constexpr bool ARITY_SATURATED_V = ::base::ACCEPTS_ANY_LENGTH_V<T>;

	/**
	 * @brief can we enumerate this type's fields?
	 * @details Asymmetric on purpose: false means "I do not know", never "it has no fields" - a
	 * wrong false costs a message and a wrong true costs a format. The branches are `if constexpr`
	 * and not a conjunction, because `&&` instantiates both operands and the cheap
	 * structural answers have to come first.
	 */
	template<class T>
	consteval bool canEnumerateImpl() {
		using U = ::std::remove_cv_t<T>;

		if constexpr (Access::NAMES_MEMBER_COUNT_V<U>)
			return true; /* the author said so */
		else if constexpr (::std::is_union_v<U> || ::std::is_array_v<U> || !::std::is_class_v<U>)
			return false;
		else if constexpr (!::std::is_aggregate_v<U>)
			return false; /* cannot probe a non-aggregate */
		/**
		 * @brief No check for a polymorphic type: a class with virtual functions is never an
		 * aggregate, so the line above has already refused it.
		 */
		else if constexpr (::base::HAS_BASE_V<U>)
			return false; /* the probe would count the base */
		else if constexpr (::std::is_empty_v<U>)
			return true;  /* zero fields, and that is known */
		else
			/*
			 * Zero is not a count for a type that has fields - it means only the empty
			 * clause list compiled - and a number past the ladder's limit has no rung
			 * that could walk it.
			 */
			return ::base::ELIDED_ARITY_V<U> != ::base::NO_ARITY && ::base::ELIDED_ARITY_V<U> != 0
			    && ::base::ELIDED_ARITY_V<U> <= MAX_MEMBERS;
	}

} /* namespace ser::internal */

namespace ser {

	/** @brief the public field-count surface */
	template<class T>
	inline constexpr bool CAN_ENUMERATE_MEMBERS_V
		= internal::canEnumerateImpl<::std::remove_cv_t<T>>();

	namespace internal {
		template<class T>
		consteval ::std::size_t memberCountOf() {
			if constexpr (Access::NAMES_MEMBER_COUNT_V<T>)
				return Access::declaredMemberCount<T>();
			else if constexpr (::std::is_empty_v<T>)
				return 0;
			else
				return ::base::ELIDED_ARITY_V<T>;
		}
	}

	/**
	 * @brief Only ask this when CAN_ENUMERATE_MEMBERS_V<T> is true. It is a plain value rather
	 * than a hard error otherwise, because dispatch needs to test the guard first.
	 */
	template<class T>
	inline constexpr ::std::size_t MEMBER_COUNT_V
		= internal::memberCountOf<::std::remove_cv_t<T>>();

	/**
	 * @brief visiting
	 * @details The arity lives here rather than in ser::Access because the count needs the ladder
	 * (MemberTypeT below is built out of it) and the ladder must therefore need
	 * nothing. access::visitMembersN takes N explicitly for exactly that reason.
	 */
	template<class T, class F>
	constexpr decltype(auto) visitMembers(T&& obj, F&& f) {
		using U = ::std::remove_cvref_t<T>;
		/*
		 * A declared count is NOT compared against the probe, because the probe errs in both
		 * directions - a C array field overshoots, and the scan undershoots on a member no
		 * clause can initialize. A wrong ser::Members<N> is caught where it cannot be wrong
		 * about anything: the arity of a structured binding is CHECKED against the type, so
		 * it is a compile error in the rung it selects.
		 */
		static_assert(
			CAN_ENUMERATE_MEMBERS_V<U>,
			"ser: cannot enumerate this type's fields. Declare the count in the class:\n"
			"  using ser_members = ser::Members<N>;   (with SER_FRIEND for private fields)\n"
			"or give the type a hook: serVisit, or serWrite + serRead.\n"
			"A C array field is the common reason: it elides braces, so the count comes "
			"out too large and ser::Members<N> is what settles it."
		);
		return Access::visitMembersN<MEMBER_COUNT_V<U>>(
			::std::forward<T>(obj), ::std::forward<F>(f)
		);
	}

	/**
	 * @brief MemberTypeT
	 * @details The Boost.PFR trick: tie the members into a tuple of references and read the types
	 * back off it. Never called - only its return type is ever needed - so the
	 * declval<T&> is harmless even for a type that cannot be created.
	 */
	template<class T>
	constexpr auto tieMembers(T& obj) {
		return Access::visitMembersN<MEMBER_COUNT_V<T>>(obj, [](auto&... fields) {
			return ::std::tie(fields...);
		});
	}

	template<class T>
	using MemberTupleT = decltype(tieMembers(::std::declval<::std::remove_cv_t<T>&>()));

	/*
	 * The stripping here is why FieldDeclsT below exists at all. A field's DECLARED type
	 * survives only as long as the binding does: handing one to an `auto&` parameter turns
	 * both a `const int&` member and a `const int` member into `const int&`. So no guard
	 * downstream of the walk can tell a reference field from a const one - and one of those
	 * two is a shape that works, which is why the guards ask FieldDeclsT instead.
	 */
	namespace internal {
		/**
		 * @brief FieldDeclsT -> the same fields with cv and references stripped, in both the
		 * shapes the rest of the library asks for.
		 */
		template<class L>
		struct DeclFieldTypes;

		template<class... Ds>
		struct DeclFieldTypes<::base::TypeList<Ds...>> {
			using Tuple = ::std::tuple<::std::remove_cvref_t<typename Ds::type>...>;
			using List  = ::base::TypeList<::std::remove_cvref_t<typename Ds::type>...>;
		};
	}

	/**
	 * @brief The same fields as FieldTypesT, but as they were DECLARED - `int&` for a
	 * reference member, `const int` for a const one, plus whether a non-const lvalue
	 * reference can bind to each.
	 *
	 * Only the guards use this. Every serializing path keeps using FieldTypesT, which
	 * must stay stripped: a `const int` field has to dispatch as `int`.
	 */
	template<class T>
	using FieldDeclsT = decltype(Access::fieldDeclsN<MEMBER_COUNT_V<::std::remove_cv_t<T>>>(
		::std::declval<::std::remove_cv_t<T>&>()
	));

	/**
	 * @brief Taken off FieldDeclsT and not off MemberTupleT: MemberTupleT goes through
	 * std::tie, which needs a non-const reference per field, and nothing binds one to a
	 * BIT-FIELD. FieldDeclsT asks decltype on the binding instead, which works for every
	 * field there is. It matters because the BUILD path needs these types, and that path is
	 * exactly what reads a type the walk cannot fill.
	 */
	template<class T, ::std::size_t I>
	using MemberTypeT
		= ::std::tuple_element_t<I, typename internal::DeclFieldTypes<FieldDeclsT<T>>::Tuple>;

	/**
	 * @brief The same types as a pack rather than one index at a time - what a caller needs when
	 * it wants every field type at once, and what dispatch hands to the build path.
	 */
	template<class T>
	using FieldTypesT = typename internal::DeclFieldTypes<FieldDeclsT<T>>::List;

} /* namespace ser */
