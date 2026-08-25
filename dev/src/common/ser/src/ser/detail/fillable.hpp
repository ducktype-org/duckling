#pragma once

// ── fill, or build? ───────────────────────────────────────────────────────────
// Every adapter that holds other objects - a vector, an optional, a map, a Box - asks the
// same two questions about the type it holds, and the answers decide both which code runs
// and whether the adapter has a read path at all.
//
//   FILL_IN_PLACE_V<T>     the element can be brought into existence empty (resize,
//                          emplace(), emplaceBack()) and then written into by
//                          dispatchRead. Zero moves, zero temporaries.
//   BUILDABLE_V<T>         it cannot, so the element is built by dispatchMake and moved
//                          into the container - which is what emplace/insert/push
//                          actually asks of T, and the one thing the fill path never
//                          needs.
//   READABLE_ELEMENT_V<T>  either of the two. That is exactly "this adapter can read a T
//                          back", so it is the requires-clause on `read` and `make`.
//
// Spelled once here rather than in each adapter, so the same conjunction cannot drift
// between five copies.
//
// ── why these belong on the member and not on the specialization ──────────────
// None of this constrains WRITING: a type that can be written and not read is an ordinary
// shape, and the write side needs neither a default constructor nor a move. So the
// constraint goes on `read` and `make`, which leaves the specialization selected and lets
// checkHooks report "this type can be written but never read back" instead of the
// specialization vanishing and dispatch reporting the much vaguer "no way to serialize".

#include <concepts>
#include <type_traits>

namespace ser::detail {

	/** @brief Can dispatchRead write into an element that already exists? */
	template<class T>
	inline constexpr bool FILL_IN_PLACE_V
		= ::std::default_initializable<T> && ::std::is_move_assignable_v<T>;

	/**
	 * @brief Can a fresh element be built by dispatchMake and moved into the container?
	 * @note is_move_constructible_v rather than the std::move_constructible concept: the
	 * concept also demands a nothrow destructor, which no emplace here needs.
	 */
	template<class T>
	inline constexpr bool BUILDABLE_V = ::std::is_move_constructible_v<T>;

	/** @brief Either path exists, so the adapter has a read side for this element. */
	template<class T>
	inline constexpr bool READABLE_ELEMENT_V = FILL_IN_PLACE_V<T> || BUILDABLE_V<T>;

}  // namespace ser::detail
