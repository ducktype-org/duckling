#pragma once

#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/errc.hpp>

#include <type_traits>
#include <utility>

namespace ser::builtin {

    template <class T>
    concept enum_like = ::std::is_enum_v<::std::remove_cv_t<T>>;

    // The underlying type is what goes on the wire, so changing it changes the format -
    // schema_hash (block F) is what catches that.
    //
    // No enumerator validation on read: without reflection there is no list to check
    // against, and a scoped enum holding an unlisted value is well-defined as long as
    // it fits the underlying type. Types that need validation provide their own
    // serializer<T> and return errc::invalid_value from it.

    template <class T, writer Ar>
    constexpr errc write_enum(Ar& ar, const T& v) {
        using U = ::std::remove_cv_t<T>;
        return write_scalar(ar, ::std::to_underlying(static_cast<U>(v)));
    }

    template <class T, reader Ar>
    constexpr errc read_enum(Ar& ar, T& v) {
        using U = ::std::underlying_type_t<::std::remove_cv_t<T>>;
        U raw{};
        if (const auto e = read_scalar(ar, raw); e != errc::ok) return e;
        v = static_cast<::std::remove_cv_t<T>>(raw);
        return errc::ok;
    }

} // namespace ser::builtin
