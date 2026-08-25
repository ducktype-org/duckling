#pragma once

// ── MIN_WIRE_SIZE_V<T> ────────────────────────────────────────────────────────
// The fewest bytes T can possibly occupy in a stream. It exists for one job: `n` elements
// cannot be there unless `n * MIN_WIRE_SIZE_V<E>` bytes are, and without that arithmetic a
// corrupt length prefix is an allocation.
//
// A LOWER bound, never an estimate: anything this cannot measure answers 1, because a bound
// that is too small only weakens the check while one that is too large would reject a
// perfectly valid stream. A class template rather than a function, so the std adapters can
// specialize it - a vector's minimum is its prefix, whatever the element is.

#include <ser/access.hpp>
#include <ser/builtin/array.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/config.hpp>
#include <ser/detail/describe.hpp>
#include <ser/detail/meta.hpp>

#include <cstddef>
#include <type_traits>

namespace ser {

	template<class T>
	struct min_wire_size;

	template<class T>
	inline constexpr ::std::size_t MIN_WIRE_SIZE_V = min_wire_size<::std::remove_cv_t<T>>::VALUE;

	namespace detail {

		template<class... Fs>
		[[nodiscard]] consteval ::std::size_t sumMinWire(type_list<Fs...>) {
			return (::std::size_t{ 0 } + ... + MIN_WIRE_SIZE_V<Fs>);
		}

		template<class T>
		[[nodiscard]] consteval ::std::size_t minWireCompute() {
			// bool is one byte on the wire whatever sizeof(bool) is on this platform:
			// the object representation never reaches the stream.
			if constexpr (::std::is_same_v<T, bool>)
				return 1;
			else if constexpr (builtin::scalar_like<T> || builtin::enum_like<T>)
				return sizeof(T);
			else if constexpr (builtin::array_like<T>)
				return builtin::ARRAY_LENGTH_V<T> * MIN_WIRE_SIZE_V<builtin::array_element_t<T>>;
			else if constexpr (access::HAS_SCHEMA_AS_V<T>)
				return MIN_WIRE_SIZE_V<::std::remove_cv_t<access::schema_as_t<T>>>;
			else if constexpr (::std::is_empty_v<T>)
				return 0;  // truthful: an empty type writes nothing
			else if constexpr (CAN_ENUMERATE_MEMBERS_V<T>)
				return sumMinWire(field_types_t<T>{});
			else
				return 1;  // a hook, a container, anything unmeasured
		}

	}  // namespace detail

	template<class T>
	struct min_wire_size {
		static constexpr ::std::size_t VALUE = detail::minWireCompute<T>();
	};

}  // namespace ser
