#pragma once

/*
 * MIN_WIRE_SIZE_V<T>
 * A lower bound on the number of bytes a serialized T takes. Reading a container with
 * length n, ser rejects the length when fewer than n * MIN_WIRE_SIZE_V<E> bytes are left, so
 * a corrupt length cannot trigger a huge allocation. A type it cannot measure gets 1.
 */

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>

#include <ser/builtin/enum.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/config.hpp>
#include <ser/internal/access_detect.hpp>
#include <ser/internal/describe.hpp>

#include <cstddef>
#include <type_traits>

namespace ser {

	template<class T>
	struct min_wire_size;

	template<class T>
	inline constexpr ::std::size_t MIN_WIRE_SIZE_V = min_wire_size<::std::remove_cv_t<T>>::VALUE;

	/**
	 * @internal How `min_wire_size` is computed. Kept in this header because the public half
	 * is a single trait over it: a separate file would leave an eight-line public header that
	 * says nothing on its own.
	 */
	namespace internal {

		template<class... Fs>
		[[nodiscard]] consteval ::std::size_t sumMinWire(::base::TypeList<Fs...>) {
			return (::std::size_t{ 0 } + ... + MIN_WIRE_SIZE_V<Fs>);
		}

		template<class T>
		[[nodiscard]] consteval ::std::size_t minWireCompute() {
			/**
			 * @brief bool is one byte on the wire whatever sizeof(bool) is on this platform:
			 * the object representation never reaches the stream.
			 */
			if constexpr (::std::is_same_v<T, bool>)
				return 1;
			else if constexpr (builtin::scalar_like<T> || builtin::enum_like<T>)
				return sizeof(T);
			else if constexpr (access::HAS_WIRE_AS_V<T>)
				return MIN_WIRE_SIZE_V<::std::remove_cv_t<access::wire_as_t<T>>>;
			else if constexpr (::std::is_empty_v<T>)
				return 0; /* truthful: an empty type writes nothing */
			else if constexpr (access::HAS_DESCRIBED_V<T>)
				return sumMinWire(described_types_t<T>{});
			else if constexpr (CAN_ENUMERATE_MEMBERS_V<T>)
				return sumMinWire(field_types_t<T>{});
			else
				return 1; /* a hook, a container, anything unmeasured */
		}

	} /* namespace internal */

	template<class T>
	struct min_wire_size {
		static constexpr ::std::size_t VALUE = internal::minWireCompute<T>();
	};

} /* namespace ser */
