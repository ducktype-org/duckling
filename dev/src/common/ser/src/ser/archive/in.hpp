#pragma once

#include <ser/archive/archive_base.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/debug.hpp>
#include <ser/errc.hpp>
#include <ser/pool/context.hpp>

#include <ser/detail/bytes.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/meta.hpp>

#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

    // Shares position(), the depth counter, pool<P>() and the context storage with
    // ser::out through archive_base.
    template <class Ctx = no_context>
    class in : public detail::archive_base<Ctx> {
        using base = detail::archive_base<Ctx>;
        using base::pos;          // the base is dependent, so pos is not found unqualified

        ::std::span<const ::std::byte> bytes;

    public:
        static constexpr bool is_reading = true;
        using context_type = Ctx;

        constexpr explicit in(::std::span<const ::std::byte> b) noexcept
            requires (::std::is_empty_v<Ctx>)
            : bytes(b) {}

        constexpr in(::std::span<const ::std::byte> b, Ctx& c) noexcept
            : base(c), bytes(b) {}

        // ── deserialization entry point ───────────────────────────────────────
        // The requires clause turns "you passed a const object" into one readable
        // line instead of a wall of template errors deep inside dispatch.
        template <class... Ts>
            requires (... && !::std::is_const_v<Ts>)
        constexpr errc operator()(Ts&... xs) {
            errc e = errc::ok;
            (void)(((e = detail::dispatch_read<Ts>(*this, xs)) == errc::ok) && ...);
            return e;
        }

        // ── raw byte access ───────────────────────────────────────────────────
        // One copy straight into the destination - see load_into in detail/bytes.hpp.
        template <class T>
        constexpr errc read_raw(T& dst) {
            if (const auto e = ensure(sizeof(T)); e != errc::ok) return e;
            detail::load_into(dst, take(sizeof(T)).data());
            return errc::ok;
        }

        [[nodiscard]] constexpr ::std::size_t avail() const noexcept {
            return bytes.size() - pos;
        }

        [[nodiscard]] constexpr errc ensure(::std::size_t n) const noexcept {
            return n <= avail() ? errc::ok : errc::truncated;
        }

        // Precondition: ensure(n) == errc::ok. Kept separate so the bounds check can be
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

} // namespace ser
