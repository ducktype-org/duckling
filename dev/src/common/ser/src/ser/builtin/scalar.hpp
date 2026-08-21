#pragma once

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <cstddef>
#include <type_traits>

namespace ser::builtin {

    template <class T>
    concept scalar_like = ::std::is_arithmetic_v<::std::remove_cv_t<T>>
                       || ::std::is_same_v<::std::remove_cv_t<T>, ::std::byte>;

    // ── bool is not "just one byte" ───────────────────────────────────────────
    // sizeof(bool) is implementation-defined and a bool holding anything other than
    // 0 or 1 is undefined behaviour - reading one back with memcpy from a corrupted
    // stream would poison every later branch on it. So bool goes on the wire as an
    // explicit 0/1 byte and comes back validated. This is the difference between
    // a fuzzer finding errc::invalid_value and a fuzzer finding a miscompile.

    template <class T, writer Ar>
    constexpr errc write_scalar(Ar& ar, const T& v) {
        using U = ::std::remove_cv_t<T>;
        if constexpr (::std::is_same_v<U, bool>) {
            return ar.write_raw(static_cast<::std::byte>(v ? 1 : 0));
        } else {
            return ar.write_raw(v);
        }
    }

    template <class T, reader Ar>
    constexpr errc read_scalar(Ar& ar, T& v) {
        using U = ::std::remove_cv_t<T>;
        if constexpr (::std::is_same_v<U, bool>) {
            ::std::byte b{};
            if (const auto e = ar.read_raw(b); e != errc::ok) return e;
            if (b != ::std::byte{0} && b != ::std::byte{1}) return errc::invalid_value;
            v = (b == ::std::byte{1});
            return errc::ok;
        } else {
            return ar.read_raw(v);
        }
    }

    template <class T>
    inline constexpr ::std::size_t scalar_wire_size =
        ::std::is_same_v<::std::remove_cv_t<T>, bool> ? ::std::size_t{1} : sizeof(T);

} // namespace ser::builtin
