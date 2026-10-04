#pragma once

/*
 * std::array<T, N> and T[N]
 * The elements in order, nothing else: the extent is part of the type, so no length goes on
 * the wire. Each element goes through full dispatch rather than a bulk copy - an element may
 * have its own hook, and the wire has neither padding nor the platform's alignment.
 *
 * Only write and read. An array of elements that cannot be filled in place is built through
 * aggregate initialization instead, one clause per element, which is the same bytes.
 */

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ser {

	namespace internal {

		/**
		 * @brief Base of the two array serializers, so a C array field can tell them from a
		 * serializer a user wrote for that array type.
		 */
		struct elementwise_array {};

		/**
		 * @brief The extent then the element. The same for both kinds of array, because they
		 * are the same bytes.
		 */
		template<class E, ::std::size_t N, class Mode, class Seen>
		[[nodiscard]] consteval ::std::uint64_t schemaArray(::std::uint64_t h) {
			return schemaOf<::std::remove_cv_t<E>, Mode, Seen>(
				schemaNumber(schemaText(h, "array"), N)
			);
		}

	} /* namespace internal */

	template<class T, ::std::size_t N>
	struct min_wire_size<::std::array<T, N>> {
		static constexpr ::std::size_t VALUE = N * MIN_WIRE_SIZE_V<T>;
	};

	template<class T, ::std::size_t N>
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	struct min_wire_size<T[N]> {
		static constexpr ::std::size_t VALUE = N * MIN_WIRE_SIZE_V<T>;
	};

	template<class T, ::std::size_t N>
	struct schema<::std::array<T, N>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaArray<T, N, Mode, Seen>(h);
		}
	};

	template<class T, ::std::size_t N>
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	struct schema<T[N]> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaArray<T, N, Mode, Seen>(h);
		}
	};

	template<class T, ::std::size_t N>
	struct serializer<::std::array<T, N>>: internal::elementwise_array {
		static constexpr Errc write(writer auto& ar, const ::std::array<T, N>& a) {
			for (const auto& e: a)
				if (const auto c = internal::dispatchWrite<T>(ar, e); c != Errc::Ok) return c;
			return Errc::Ok;
		}

		static constexpr Errc read(reader auto& ar, ::std::array<T, N>& a) {
			for (auto& e: a)
				if (const auto c = internal::dispatchRead<T>(ar, e); c != Errc::Ok) return c;
			return Errc::Ok;
		}
	};

	template<class T, ::std::size_t N>
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	struct serializer<T[N]>: internal::elementwise_array {
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
		static constexpr Errc write(writer auto& ar, const T (&a)[N]) {
			for (const auto& e: a)
				if (const auto c = internal::dispatchWrite<T>(ar, e); c != Errc::Ok) return c;
			return Errc::Ok;
		}

		// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
		static constexpr Errc read(reader auto& ar, T (&a)[N]) {
			for (auto& e: a)
				if (const auto c = internal::dispatchRead<T>(ar, e); c != Errc::Ok) return c;
			return Errc::Ok;
		}
	};

} /* namespace ser */
