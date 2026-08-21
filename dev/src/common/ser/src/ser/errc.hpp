#pragma once

#include<ser/config.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace ser {

    enum class errc : ::std::uint8_t {
        ok = 0,
        truncated,
        buffer_full,
        unexpected_end,

        size_overflow,
        message_size,
        invalid_value,
        misaligned,
        depth_exceeded,

        bad_magic,
        schema_mismatch,
        platform_mismatch,
        checksum_failed,

        pool_missing,
        pool_modified,
        dangling_ref,
        double_take,

        io_error,
    };

    [[nodiscard]] constexpr const char* to_string(errc e) noexcept {
    switch (e) {
        case errc::ok:                return "ok";
        case errc::truncated:         return "stream truncated";
        case errc::buffer_full:       return "no space left in buffer";
        case errc::unexpected_end:    return "unexpected end of data";
        case errc::size_overflow:     return "length arithmetic overflowed";
        case errc::message_size:      return "container size limit exceeded";
        case errc::invalid_value:     return "invalid value in stream";
        case errc::misaligned:        return "misaligned address for a view";
        case errc::depth_exceeded:    return "maximum depth exceeded";
        case errc::bad_magic:         return "bad magic";
        case errc::schema_mismatch:   return "schema_hash mismatch";
        case errc::platform_mismatch: return "platform mismatch";
        case errc::checksum_failed:   return "checksum failed";
        case errc::pool_missing:      return "required pool missing from context";
        case errc::pool_modified:     return "pool modified after the write started";
        case errc::dangling_ref:      return "Ref without a matching Box";
        case errc::double_take:       return "same index taken twice";
        case errc::io_error:          return "I/O error";
    }
    return "unknown error";
}

    struct error {
        errc          code     = errc::ok;
        ::std::size_t position = 0;
    
        [[nodiscard]] ::std::string message() const {
            return ::std::string{to_string(code)} + " (at position " + ::std::to_string(position) + ")";
        }
        friend constexpr bool operator==(const error&, const error&) = default;
    };

    class exception : public ::std::runtime_error {
        error e;
    public:
        explicit exception(error err) : ::std::runtime_error(err.message()), e(err) {}
        [[nodiscard]] const error& err()  const noexcept { return e; }
        [[nodiscard]] errc         code() const noexcept { return e.code; }
    };

    [[noreturn]] inline void throw_error(error e) { throw exception{e}; }
    [[noreturn]] inline void throw_error(errc c, ::std::size_t pos = 0) { throw_error(error{c, pos}); }

    template <class T = void>
    class [[nodiscard]] result {
        ::std::expected<T, error> e{};
    public:
        using value_type = T;

        constexpr result() = default;

        template <class U = T>
            requires (!::std::is_void_v<T>
                    && !::std::same_as<::std::remove_cvref_t<U>, error>
                    && !::std::same_as<::std::remove_cvref_t<U>, errc>
                    && !::std::same_as<::std::remove_cvref_t<U>, result>
                    && ::std::constructible_from<T, U>)
        constexpr result(U&& v) : e(::std::forward<U>(v)) {}

        constexpr result(error err) : e(::std::unexpected(err)) {}
        constexpr result(errc c, ::std::size_t pos = 0) : e(::std::unexpected(error{c, pos})) {}

        [[nodiscard]] constexpr bool has_value() const noexcept { return e.has_value(); }

        constexpr explicit operator bool()       const noexcept { return e.has_value(); }

        [[nodiscard]] constexpr const error& err() const { return e.error(); }

        [[nodiscard]] constexpr errc code() const noexcept {
            return e.has_value() ? errc::ok : e.error().code;
        }

        constexpr decltype(auto) operator*() &&      { return *::std::move(e); }
        constexpr decltype(auto) operator*() &       { return *e; }
        constexpr decltype(auto) operator*() const&  { return *e; }

        constexpr auto operator->()       { return &*e; }
        constexpr auto operator->() const { return &*e; }

        constexpr decltype(auto) or_throw() && {
            if (!e) throw_error(e.error());
            if constexpr (!::std::is_void_v<T>) return *::std::move(e);
        }

        constexpr decltype(auto) or_throw() & {
            if (!e) throw_error(e.error());
            if constexpr (!::std::is_void_v<T>) return *e;
        }

    };

    // Boundary between the internal layer (errc) and the public one (result).
    [[nodiscard]] constexpr result<> as_result(errc c, ::std::size_t pos = 0) noexcept {
        return c == errc::ok ? result<>{} : result<>{c, pos};
    }

} // namespace ser
