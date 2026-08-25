#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

namespace ser::detail {

	template<class...>
	inline constexpr bool DEPENDENT_FALSE = false;

	template<class T>
	struct type_tag {
		using type = T;
	};

	template<class... Ts>
	struct type_list {
		static constexpr ::std::size_t SIZE = sizeof...(Ts);
	};

	// One field as the ladder saw it, before anything strips anything. `type` is the
	// DECLARED type - `int&` for a reference member, `const int` for a const one - which is
	// a distinction no function parameter can carry, because an expression never has
	// reference type. `BINDABLE` answers "can a non-const lvalue reference bind to this
	// member", and exactly one thing answers no: a bit-field.
	template<class T, bool Bindable>
	struct field_decl {
		using type                     = T;
		static constexpr bool BINDABLE = Bindable;
	};

	template<class T>
	struct is_type_list: ::std::false_type {};

	template<class... Ts>
	struct is_type_list<type_list<Ts...>>: ::std::true_type {};

	namespace impl {
		template<class T>
		struct tag_of {};

		template<class... Ts>
		struct tag_set: tag_of<Ts>... {};
	}

	// ── concatenating and repeating type lists ────────────────────────────────
	// Needed to spell "this field contributes N initializer clauses" - see
	// flatten_field in dispatch.hpp.
	template<class... Ls>
	struct cat_lists;

	template<>
	struct cat_lists<> {
		using type = type_list<>;
	};

	template<class... A>
	struct cat_lists<type_list<A...>> {
		using type = type_list<A...>;
	};

	template<class... A, class... B, class... Rest>
	struct cat_lists<type_list<A...>, type_list<B...>, Rest...>:
		  cat_lists<type_list<A..., B...>, Rest...> {};

	template<class... Ls>
	using cat_lists_t = typename cat_lists<Ls...>::type;

	// The index is there only to give the pack something to expand over.
	template<class L, ::std::size_t>
	using ignore_index_t = L;

	template<class L, class Seq>
	struct repeat_list;

	template<class L, ::std::size_t... I>
	struct repeat_list<L, ::std::index_sequence<I...>> {
		using type = cat_lists_t<ignore_index_t<L, I>...>;
	};

	template<class L, ::std::size_t N>
	using repeat_list_t = typename repeat_list<L, ::std::make_index_sequence<N>>::type;

	template<class T, class List>
	struct contains;

	template<class T, class... Ts>
	struct contains<T, type_list<Ts...>>:
		  ::std::bool_constant<::std::is_base_of_v<impl::tag_of<T>, impl::tag_set<Ts...>>> {};

	template<class T, class List>
	inline constexpr bool CONTAINS_V = contains<T, List>::value;

	inline constexpr ::std::size_t LIST_NPOS = static_cast<::std::size_t>(-1);

	template<class T, class List>
	struct index_of;

	template<class T, class... Ts>
	struct index_of<T, type_list<Ts...>> {
		static constexpr ::std::size_t VALUE = [] {
			// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
			constexpr bool HITS[] = { ::std::is_same_v<T, Ts>..., false };
			for (::std::size_t i = 0; i < sizeof...(Ts); ++i)
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				if (HITS[i]) return i;
			return LIST_NPOS;
		}();
	};

	template<class T, class List>
	inline constexpr ::std::size_t INDEX_OF_V = index_of<T, List>::VALUE;

}  // namespace ser::detail
