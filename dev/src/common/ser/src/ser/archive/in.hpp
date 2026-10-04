#pragma once

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>

#include <ser/archive/archive_base.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/internal/bytes.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/pool/context.hpp>

#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

	/**
	 * @brief Shares position(), the depth counter, pool<P>() and the context storage with
	 * ser::Out through ArchiveBase.
	 */
	template<class Ctx = NoContext>
	class In final: public internal::ArchiveBase<Ctx> {
		using Base = internal::ArchiveBase<Ctx>;
		using Base::pos; /* the base is dependent, so pos is not found unqualified */

		::std::span<const ::std::byte> bytes;

	public:
		static constexpr bool IS_READING = true;
		using ContextType                = Ctx;

		constexpr explicit In(::std::span<const ::std::byte> b) noexcept
			requires(::std::is_empty_v<Ctx>)
			  : bytes(b) {}

		constexpr In(::std::span<const ::std::byte> b, Ctx& c) noexcept: Base(c), bytes(b) {}

		/**
		 * @brief deserialization entry point
		 * @details The requires clause turns "you passed a const object" into one readable
		 * line instead of a wall of template errors deep inside dispatch.
		 */
		template<class... Ts>
		requires(... && !::std::is_const_v<Ts>) constexpr Errc operator()(Ts&... xs) {
			Errc e = Errc::Ok;
			(void) (((e = internal::dispatchRead<Ts>(*this, xs)) == Errc::Ok) && ...);
			return e;
		}

		/**
		 * @brief raw byte access
		 * @details One copy straight into the destination - see loadInto in internal/bytes.hpp.
		 */
		template<class T>
		constexpr Errc readRaw(T& dst) {
			if (const auto e = ensure(sizeof(T)); e != Errc::Ok) return e;
			internal::loadInto(dst, take(sizeof(T)).data());
			return Errc::Ok;
		}

		[[nodiscard]] constexpr ::std::size_t avail() const noexcept { return bytes.size() - pos; }

		[[nodiscard]] constexpr Errc ensure(::std::size_t n) const noexcept {
			return n <= avail() ? Errc::Ok : Errc::Truncated;
		}

		/**
		 * @brief Precondition: ensure(n) == Errc::Ok. Kept separate so the bounds check can be
		 * hoisted out of a loop once, instead of paid per element.
		 * Not noexcept: CORE_ASSERT throws base::Panic in a Dev build.
		 */
		[[nodiscard]] constexpr ::std::span<const ::std::byte> take(::std::size_t n) {
			CORE_ASSERT(n <= avail(), "ser::In::take: called without a successful ensure()");
			const auto chunk = bytes.subspan(pos, n);
			pos += n;
			return chunk;
		}

		/** @brief position */
		[[nodiscard]] constexpr ::std::size_t size() const noexcept { return bytes.size(); }

		/** @brief Not noexcept: CORE_ASSERT throws base::Panic in a Dev build. */
		constexpr void reset(::std::size_t p = 0) {
			CORE_ASSERT(p <= bytes.size(), "ser::In::reset: position past the end of the stream");
			pos = p;
		}
	};

	/* ser::In{span} without naming the context. */
	In(::std::span<const ::std::byte>) -> In<NoContext>;

} /* namespace ser */
