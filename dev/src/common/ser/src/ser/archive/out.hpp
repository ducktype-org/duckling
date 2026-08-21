#pragma once

#include <ser/archive/archive_base.hpp>
#include <ser/archive/buffer.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/debug.hpp>
#include <ser/errc.hpp>
#include <ser/pool/context.hpp>

#include <ser/detail/bytes.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/meta.hpp>
#include <ser/detail/ovf.hpp>

#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

    // position(), depth()/push_depth()/pop_depth(), pool<P>() and the context storage
    // come from archive_base - see the note there on why they are shared.
    template <class Buf, class Ctx = no_context>
    class out : public detail::archive_base<Ctx> {
        static_assert(byte_buffer<Buf>,
                      "ser::out: Buf must expose data() -> std::byte* and size(). "
                      "Use std::vector<std::byte> (growable) or std::span<std::byte> (fixed).");

        using base = detail::archive_base<Ctx>;
        using base::pos;          // the base is dependent, so pos is not found unqualified
        using base::context;

        Buf& buf;

    public:
        static constexpr bool is_writing = true;
        using buffer_type  = Buf;
        using context_type = Ctx;

        // ── where writing starts ──────────────────────────────────────────────
        // A growable buffer is APPENDED to: a fresh archive begins at buf.size(), so
        // several independent writes accumulate into one stream and nothing already in
        // the buffer is overwritten. A fixed buffer is filled from the front, because
        // its size() is capacity and carries no notion of how much is already written -
        // to continue inside one, keep the archive and keep calling it.
        //
        // The consequence worth knowing: position() is an offset into the BUFFER, not
        // into the message, so an error position points at the byte that failed. reset()
        // takes buffer offsets too - capture position() before writing a header and
        // hand that same value back (M2).
        constexpr explicit out(Buf& b) noexcept
            requires (::std::is_empty_v<Ctx>)
            : buf(b) { pos = detail::buffer_origin(b); }

        constexpr out(Buf& b, Ctx& c) noexcept : base(c), buf(b) { pos = detail::buffer_origin(b); }

        // ── serialization entry point ─────────────────────────────────────────
        // Stops at the first failure; the returned code belongs to that argument.
        template <class... Ts>
        constexpr errc operator()(const Ts&... xs) {
            errc e = errc::ok;
            (void)(((e = detail::dispatch_write<Ts>(*this, xs)) == errc::ok) && ...);
            return e;
        }

        // ── raw byte access ───────────────────────────────────────────────────
        // One copy straight into the buffer - no temporary, unlike raw_write of a
        // locally materialized byte array.
        template <class T>
        constexpr errc write_raw(const T& v) {
            if (const auto e = reserve(sizeof(T)); e != errc::ok) return e;
            detail::store(detail::buffer_data(buf) + pos, v);
            pos += sizeof(T);
            return errc::ok;
        }

        constexpr errc raw_write(::std::span<const ::std::byte> src) {
            if (const auto e = reserve(src.size()); e != errc::ok) return e;
            detail::copy_bytes(detail::buffer_data(buf) + pos, src.data(), src.size());
            pos += src.size();
            return errc::ok;
        }

        // `n` is an increment on top of the current position, not an absolute size.
        constexpr errc reserve(::std::size_t n) {
            ::std::size_t need = 0;
            if (detail::add_ovf(pos, n, need)) return errc::size_overflow;
            return detail::buffer_ensure(buf, need);
        }

        constexpr errc pad_to(::std::size_t align) {
            SER_ASSERT(align != 0 && (align & (align - 1)) == 0,
                       "ser::out::pad_to: alignment must be a power of two");
            const ::std::size_t n = detail::align_up(pos, align) - pos;
            if (n == 0) return errc::ok;
            if (const auto e = reserve(n); e != errc::ok) return e;
            detail::fill_bytes(detail::buffer_data(buf) + pos, ::std::byte{0}, n);
            pos += n;
            return errc::ok;
        }

        // ── position ──────────────────────────────────────────────────────────
        // M2 rewinds to patch the header after the payload size is known. The bound is
        // the buffer, which is why reset() is not shared with ser::in.
        constexpr void reset(::std::size_t p = 0) noexcept {
            SER_ASSERT(p <= detail::buffer_size(buf),
                       "ser::out::reset: position past the end of the buffer");
            pos = p;
        }

        // Always call it. Without pools it folds away to nothing; with pools (M2) it is
        // what actually emits them.
        constexpr errc finish() { return context().finish(); }
    };

} // namespace ser
