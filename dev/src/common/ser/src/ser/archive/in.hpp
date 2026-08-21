#pragma once

#include <ser/archive/archive_base.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/debug.hpp>
#include <ser/detail/bytes.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>
#include <ser/pool/context.hpp>

#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

	// Shares position(), the depth counter, pool<P>() and the context storage with
	// ser::out through archive_base.
	template<class Ctx = no_context>
	class in: public detail::archive_base<Ctx> {
		using base = detail::archive_base<Ctx>;
		using base::pos;  // the base is dependent, so pos is not found unqualified

		::std::span<const ::std::byte> bytes;

	public:
		static constexpr bool IS_READING = true;
		using context_type               = Ctx;

		constexpr explicit in(::std::span<const ::std::byte> b) noexcept
			requires(::std::is_empty_v<Ctx>)
			  : bytes(b) {}

		constexpr in(::std::span<const ::std::byte> b, Ctx& c) noexcept: base(c), bytes(b) {}

		// ── deserialization entry point ───────────────────────────────────────
		// The requires clause turns "you passed a const object" into one readable
		// line instead of a wall of template errors deep inside dispatch.
		template<class... Ts>
		requires(... && !::std::is_const_v<Ts>) constexpr Errc operator()(Ts&... xs) {
			Errc e = Errc::Ok;
			(void) (((e = detail::dispatchRead<Ts>(*this, xs)) == Errc::Ok) && ...);
			return e;
		}

		// ── raw byte access ───────────────────────────────────────────────────
		// One copy straight into the destination - see loadInto in detail/bytes.hpp.
		template<class T>
		constexpr Errc readRaw(T& dst) {
			if (const auto e = ensure(sizeof(T)); e != Errc::Ok) return e;
			detail::loadInto(dst, take(sizeof(T)).data());
			return Errc::Ok;
		}

		[[nodiscard]] constexpr ::std::size_t avail() const noexcept { return bytes.size() - pos; }

		[[nodiscard]] constexpr Errc ensure(::std::size_t n) const noexcept {
			return n <= avail() ? Errc::Ok : Errc::Truncated;
		}

		// Precondition: ensure(n) == Errc::Ok. Kept separate so the bounds check can be
		// hoisted out of a loop once, instead of paid per element.
		[[nodiscard]] constexpr ::std::span<const ::std::byte> take(::std::size_t n) noexcept {
			SER_ASSERT(n <= avail(), "ser::in::take: called without a successful ensure()");
			const auto chunk = bytes.subspan(pos, n);
			pos += n;
			return chunk;
		}

		// ── position ──────────────────────────────────────────────────────────
		[[nodiscard]] constexpr ::std::size_t size() const noexcept { return bytes.size(); }

		constexpr void reset(::std::size_t p = 0) noexcept {
			SER_ASSERT(p <= bytes.size(), "ser::in::reset: position past the end of the stream");
			pos = p;
		}
	};

	// ser::in{span} without naming the context.
	in(::std::span<const ::std::byte>) -> in<no_context>;

}  // namespace ser
