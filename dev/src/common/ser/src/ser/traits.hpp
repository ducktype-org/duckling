#pragma once

/*
 * MIN_SERIALIZED_SIZE_V<T>
 * A lower bound on the number of bytes a serialized T takes. Reading a container with
 * length n, ser rejects the length when fewer than n * MIN_SERIALIZED_SIZE_V<E> bytes are left, so
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
	struct MinSerializedSize;

	template<class T>
	inline constexpr ::std::size_t MIN_SERIALIZED_SIZE_V
		= MinSerializedSize<::std::remove_cv_t<T>>::VALUE;

	/**
	 * @internal How `MinSerializedSize` is computed. Kept in this header because the public half
	 * is a single trait over it: a separate file would leave an eight-line public header that
	 * says nothing on its own.
	 */
	namespace internal {

		template<class... Fs>
		[[nodiscard]] consteval ::std::size_t sumMinSerializedSize(::base::TypeList<Fs...>) {
			return (::std::size_t{ 0 } + ... + MIN_SERIALIZED_SIZE_V<Fs>);
		}

		template<class T>
		[[nodiscard]] consteval ::std::size_t computeMinSerializedSize() {
			/**
			 * @brief bool is one byte in the stream whatever sizeof(bool) is on this platform:
			 * the object representation never reaches the stream.
			 */
			if constexpr (::std::is_same_v<T, bool>)
				return 1;
			else if constexpr (builtin::ScalarLike<T> || builtin::EnumLike<T>)
				return sizeof(T);
			else if constexpr (Access::HAS_SERIALIZE_AS_V<T>)
				return MIN_SERIALIZED_SIZE_V<::std::remove_cv_t<Access::SerializeAsT<T>>>;
			else if constexpr (::std::is_empty_v<T>)
				return 0; /* truthful: an empty type writes nothing */
			else if constexpr (Access::HAS_DESCRIBED_V<T>)
				return sumMinSerializedSize(DescribedTypesT<T>{});
			else if constexpr (CAN_ENUMERATE_MEMBERS_V<T>)
				return sumMinSerializedSize(FieldTypesT<T>{});
			else
				return 1; /* a hook, a container, anything unmeasured */
		}

	} /* namespace internal */

	template<class T>
	struct MinSerializedSize {
		static constexpr ::std::size_t VALUE = internal::computeMinSerializedSize<T>();
	};

} /* namespace ser */
