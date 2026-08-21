#pragma once

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <cstddef>
#include <span>

namespace ser::detail {

    template <byte_buffer B>
    [[nodiscard]] constexpr ::std::byte* buffer_data(B& b) noexcept {
        return b.data();
    }

    template <byte_buffer B>
    [[nodiscard]] constexpr ::std::size_t buffer_size(const B& b) noexcept {
        return b.size();
    }

    // `need` is the absolute size the buffer must reach, not an increment.
    template <resizable_buffer B>
    constexpr errc buffer_ensure(B& b, ::std::size_t need) {
        if (b.size() < need) b.resize(need);
        return errc::ok;
    }

    template <fixed_buffer B>
    constexpr errc buffer_ensure(const B& b, ::std::size_t need) noexcept {
        return need <= b.size() ? errc::ok : errc::buffer_full;
    }

    // ── where a fresh archive starts writing ──────────────────────────────────
    // Same split as buffer_ensure, and for the same reason: a growable buffer's size()
    // is its CONTENT, so a new ser::out appends after it and the caller can build one
    // stream out of several independent writes. A fixed buffer's size() is its
    // CAPACITY - there is nothing to append after - so it is filled from the front.
    template <resizable_buffer B>
    [[nodiscard]] constexpr ::std::size_t buffer_origin(const B& b) noexcept {
        return b.size();
    }

    template <fixed_buffer B>
    [[nodiscard]] constexpr ::std::size_t buffer_origin(const B&) noexcept {
        return 0;
    }

    // Whether writes to this buffer can fail at all.
    template <class B>
    inline constexpr bool buffer_can_fail = !resizable_buffer<B>;

} // namespace ser::detail
