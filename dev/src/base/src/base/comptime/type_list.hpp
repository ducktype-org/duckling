#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

/**
 * @file
 * @brief A list of TYPES and the operations over it, plus the vocabulary for describing one
 * aggregate member.
 * @details `std::tuple` is the other way to carry a pack, and the wrong one here: it requires
 * its elements to be complete types and drags `<tuple>` in. `TypeList` is an empty tag that
 * never instantiates anything, so it works over incomplete types and over types that cannot be
 * constructed at all - which is what compile-time reflection over a foreign type needs.
 */

namespace base {

	/**
	 * @brief A pack of types as a single type, with nothing instantiated.
	 * @tparam Ts the elements, in order. Duplicates and incomplete types are both fine.
	 */
	template<class... Ts>
	struct TypeList final {
		/** @brief How many types the list holds. */
		static constexpr ::std::size_t SIZE = sizeof...(Ts);
	};

	/**
	 * @brief One aggregate member as declared, before anything strips anything.
	 * @tparam T the DECLARED type - `int&` for a reference member, `const int` for a const one,
	 * a distinction no function parameter can carry.
	 * @tparam Bindable whether a non-const lvalue reference binds to the member. Exactly one
	 * thing answers no: a bit-field. See `base::bindProbe`.
	 */
	template<class T, bool Bindable>
	struct FieldDecl final {
		/** @brief The member's declared type. */
		using type = T;
		/** @brief Whether a non-const lvalue reference binds to this member. */
		static constexpr bool BINDABLE = Bindable;
	};

	/**
	 * @brief "Can a non-const lvalue reference bind to this?"
	 * @details Declared and never defined: it appears only inside a requires-expression, so
	 * nothing is ever called. One thing answers no - a bit-field, for which the candidate is
	 * simply not viable, a substitution failure rather than a hard error.
	 */
	template<class X>
	void bindProbe(X&);

	namespace internal {

		template<class T>
		struct IsTypeListImpl: ::std::false_type {};

		template<class... Ts>
		struct IsTypeListImpl<TypeList<Ts...>>: ::std::true_type {};

		/**
		 * @brief One empty base per element, so "is T among them" is one is_base_of instead of a
		 * fold - the compiler answers it without instantiating a recursion per element.
		 */
		template<class T>
		struct TagOf {};

		template<class... Ts>
		struct TagSet: TagOf<Ts>... {};

		template<class... Ls>
		struct CatLists;

		template<>
		struct CatLists<> {
			using type = TypeList<>;
		};

		template<class... A>
		struct CatLists<TypeList<A...>> {
			using type = TypeList<A...>;
		};

		template<class... A, class... B, class... Rest>
		struct CatLists<TypeList<A...>, TypeList<B...>, Rest...>:
			  CatLists<TypeList<A..., B...>, Rest...> {};

		/** @brief The index is there only to give the pack something to expand over. */
		template<class L, ::std::size_t>
		using IgnoreIndexT = L;

		template<class L, class Seq>
		struct RepeatList;

		template<class L, ::std::size_t... I>
		struct RepeatList<L, ::std::index_sequence<I...>> {
			using type = typename CatLists<IgnoreIndexT<L, I>...>::type;
		};

		template<class T, class List>
		struct Contains;

		template<class T, class... Ts>
		struct Contains<T, TypeList<Ts...>>:
			  ::std::bool_constant<::std::is_base_of_v<TagOf<T>, TagSet<Ts...>>> {};

		template<class T, class List>
		struct IndexOf;

		template<class T, class... Ts>
		struct IndexOf<T, TypeList<Ts...>> {
			static constexpr ::std::size_t VALUE = [] {
				// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
				constexpr bool HITS[] = { ::std::is_same_v<T, Ts>..., false };
				for (::std::size_t i = 0; i < sizeof...(Ts); ++i)
					// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
					if (HITS[i]) return i;
				return static_cast<::std::size_t>(-1);
			}();
		};

	}  // namespace internal

	/** @brief Whether @p T is a `base::TypeList`. */
	template<class T>
	concept IsTypeList = internal::IsTypeListImpl<::std::remove_cv_t<T>>::value;

	/** @brief Every element of every list, concatenated in order. */
	template<class... Ls>
	using CatListsT = typename internal::CatLists<Ls...>::type;

	/** @brief The list @p L repeated @p N times, concatenated. */
	template<class L, ::std::size_t N>
	using RepeatListT = typename internal::RepeatList<L, ::std::make_index_sequence<N>>::type;

	/** @brief Whether @p T appears in @p List. */
	template<class T, class List>
	inline constexpr bool LIST_CONTAINS_V = internal::Contains<T, List>::value;

	/** @brief The answer `LIST_INDEX_OF_V` gives when the type is not in the list. */
	inline constexpr ::std::size_t LIST_NPOS = static_cast<::std::size_t>(-1);

	/** @brief The index of the first @p T in @p List, or `LIST_NPOS`. */
	template<class T, class List>
	inline constexpr ::std::size_t LIST_INDEX_OF_V = internal::IndexOf<T, List>::VALUE;

}  // namespace base
