#pragma once

/**
 * @file
 * @brief `SER_TEST_ROUNDTRIP` - the round-trip check for a format no hash can verify.
 * @details `SER_MAKE_FROM` catches two swapped fields of DIFFERENT types at compile time, and
 * cannot catch two swapped fields of the SAME type - that permutation compiles and simply
 * writes the wrong bytes. So this round-trips a sample, compares field by field and names the
 * field that came back wrong. It is a CONDITION of using `SER_MAKE_FROM_MEMBERS`, which has no
 * field list to check anything against.
 * @note Reached through `<ser/ser.hpp>`; this file lists the headers it needs itself rather
 * than the umbrella, because the umbrella includes it.
 */

#include <ser/access.hpp>
#include <ser/errc.hpp>
#include <ser/macros.hpp>
#include <ser/pool/context.hpp>
#include <ser/stream/read_write.hpp>

#include <cstddef>
#include <cstdio>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ser {

	inline constexpr ::std::size_t NO_FIELD = static_cast<::std::size_t>(-1);

	struct RoundtripReport final {
		Errc          code  = Errc::Ok; /* the round-trip itself */
		::std::size_t field = NO_FIELD; /* first field that came back different */
		const char*   name  = nullptr;  /* its name, when SER_DESCRIBE gave one */

		[[nodiscard]] constexpr bool ok() const noexcept {
			return code == Errc::Ok && field == NO_FIELD;
		}
	};

	namespace internal {

		/**
		 * @brief The described fields when the type says which they are, every field otherwise:
		 * SER_DESCRIBE deliberately leaves fields out, and those come back
		 * default-constructed, which is not a failure. Asked through ser::Access, because
		 * SER_DESCRIBE is normally written in a private section and a detector here would
		 * answer false and quietly compare the skipped fields too.
		 */
		template<class T>
		constexpr auto roundtripTie(const T& x) {
			if constexpr (Access::HAS_DESCRIBED_V<T>) {
				return Access::described(x);
			} else {
				/*
				 * tieMembers goes straight to the ladder, past the check visitMembers makes,
				 * so for a type nobody can enumerate it yields an EMPTY tuple and every
				 * round-trip reports as faithful. A test that cannot fail is worse than no
				 * test.
				 */
				static_assert(
					CAN_ENUMERATE_MEMBERS_V<T>,
					"ser: SER_TEST_ROUNDTRIP cannot see this type's fields, so it would "
					"compare nothing and pass. Add SER_DESCRIBE(a, b) to the class - it "
					"names the fields to compare and gives the report their names - or "
					"declare using ser_members = ser::Members<N>; with SER_FRIEND."
				);
				return tieMembers(x);
			}
		}

		template<class Tuple, ::std::size_t... I>
		constexpr ::std::size_t firstDifference(const Tuple& a, const Tuple& b, ::std::index_sequence<I...>) {
			::std::size_t bad = NO_FIELD;
			((bad == NO_FIELD && !(::std::get<I>(a) == ::std::get<I>(b)) ? (bad = I) : 0u), ...);
			return bad;
		}

		template<class T>
		constexpr ::std::size_t compareFields(const T& a, const T& b) {
			const auto ta = roundtripTie(a);
			const auto tb = roundtripTie(b);
			return firstDifference(
				ta, tb, ::std::make_index_sequence<::std::tuple_size_v<decltype(ta)>>{}
			);
		}

		template<class T>
		constexpr const char* fieldName(::std::size_t i) {
			if constexpr (Access::HAS_FIELD_NAMES_V<T>)
				return Access::fieldName<T>(i);
			else
				return nullptr;
		}

		inline bool reportRoundtrip(const char* what, const RoundtripReport& r) {
			if (r.ok()) return true;
			if (r.code != Errc::Ok) {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
				::std::printf("    SER_TEST_ROUNDTRIP(%s): %s\n", what, toString(r.code));
			} else if (r.name != nullptr) {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
				::std::printf(
					"    SER_TEST_ROUNDTRIP(%s): field %zu (%s) came back different\n",
					what,
					r.field,
					r.name
				);
			} else {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
				::std::printf(
					"    SER_TEST_ROUNDTRIP(%s): field %zu came back different "
					"(no field names - add SER_DESCRIBE to get one)\n",
					what,
					r.field
				);
			}
			return false;
		}

	} /* namespace internal */

	/**
	 * @brief Writes the sample, reads it back and compares. The throwing read rather than
	 * ser::read, so that a type which cannot be moved is testable too.
	 */
	template<class T>
	[[nodiscard]] RoundtripReport testRoundtrip(const T& sample) {
		::std::vector<::std::byte> buf;
		if (const auto w = write(buf, sample); !w.has_value())
			return RoundtripReport{ .code = codeOf(w) };

		try {
			const auto back = internal::readThrowing<T, NoContext>(
				::std::span<const ::std::byte>{ buf.data(), buf.size() }, Options{}
			);
			const auto bad = internal::compareFields(sample, *back);
			return RoundtripReport{ .code  = Errc::Ok,
				                    .field = bad,
				                    .name
				                    = bad == NO_FIELD ? nullptr : internal::fieldName<T>(bad) };
		} catch (const Exception& e) { return RoundtripReport{ .code = e.err().code }; }
	}

} /* namespace ser */

/**
 * @brief Yields true when the round-trip is faithful, and prints which field is not otherwise.
 * Meant to sit inside the project's check macro:  CHECK(SER_TEST_ROUNDTRIP(x));
 * Variadic because only PARENTHESES hide a comma from the preprocessor, and the samples
 * worth testing are braced: SER_TEST_ROUNDTRIP(Vec3{1, 2, 3}).
 */
#define SER_TEST_ROUNDTRIP(...) \
	(::ser::internal::reportRoundtrip(#__VA_ARGS__, ::ser::testRoundtrip(__VA_ARGS__)))
