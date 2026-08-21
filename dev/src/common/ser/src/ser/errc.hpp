#pragma once

#include <ser/config.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace ser {

	enum class Errc : ::std::uint8_t {
		Ok = 0,
		Truncated,
		BufferFull,
		UnexpectedEnd,

		SizeOverflow,
		MessageSize,
		InvalidValue,
		Misaligned,
		DepthExceeded,

		BadMagic,
		SchemaMismatch,
		PlatformMismatch,
		ChecksumFailed,

		PoolMissing,
		PoolModified,
		DanglingRef,
		DoubleTake,

		IoError,
	};

	[[nodiscard]] constexpr const char* toString(Errc e) noexcept {
		switch (e) {
		case Errc::Ok:
			return "Ok";
		case Errc::Truncated:
			return "stream Truncated";
		case Errc::BufferFull:
			return "no space left in buffer";
		case Errc::UnexpectedEnd:
			return "unexpected end of data";
		case Errc::SizeOverflow:
			return "length arithmetic overflowed";
		case Errc::MessageSize:
			return "container size limit exceeded";
		case Errc::InvalidValue:
			return "invalid value in stream";
		case Errc::Misaligned:
			return "Misaligned address for a view";
		case Errc::DepthExceeded:
			return "maximum depth exceeded";
		case Errc::BadMagic:
			return "bad magic";
		case Errc::SchemaMismatch:
			return "schema_hash mismatch";
		case Errc::PlatformMismatch:
			return "platform mismatch";
		case Errc::ChecksumFailed:
			return "checksum failed";
		case Errc::PoolMissing:
			return "required pool missing from context";
		case Errc::PoolModified:
			return "pool modified after the write started";
		case Errc::DanglingRef:
			return "Ref without a matching Box";
		case Errc::DoubleTake:
			return "same index taken twice";
		case Errc::IoError:
			return "I/O error";
		}
		return "unknown error";
	}

	struct error {
		Errc          code     = Errc::Ok;
		::std::size_t position = 0;

		[[nodiscard]] ::std::string message() const {
			return ::std::string{ toString(code) } + " (at position " + ::std::to_string(position)
			     + ")";
		}

		friend constexpr bool operator==(const error&, const error&) = default;
	};

	class exception: public ::std::runtime_error {
		error e;

	public:
		explicit exception(error err): ::std::runtime_error(err.message()), e(err) {}

		[[nodiscard]] const error& err() const noexcept { return e; }

		[[nodiscard]] Errc code() const noexcept { return e.code; }
	};

	[[noreturn]] inline void throwError(error e) { throw exception{ e }; }

	[[noreturn]] inline void throwError(Errc c, ::std::size_t pos = 0) {
		throwError(error{ .code = c, .position = pos });
	}

	template<class T = void>
	class [[nodiscard]] result {
		::std::expected<T, error> e{};

	public:
		using value_type = T;

		constexpr result() = default;

		template<class U = T>
		requires(!::std::is_void_v<T> && !::std::same_as<::std::remove_cvref_t<U>, error> && !::std::same_as<::std::remove_cvref_t<U>, Errc> && !::std::same_as<::std::remove_cvref_t<U>, result> && ::std::constructible_from<T, U>)
		constexpr result(U&& v): e(::std::forward<U>(v)) {}

		constexpr result(error err): e(::std::unexpected(err)) {}

		constexpr result(Errc c, ::std::size_t pos = 0):
			  e(::std::unexpected(error{ .code = c, .position = pos })) {}

		[[nodiscard]] constexpr bool hasValue() const noexcept { return e.has_value(); }

		constexpr explicit operator bool() const noexcept { return e.has_value(); }

		[[nodiscard]] constexpr const error& err() const { return e.error(); }

		[[nodiscard]] constexpr Errc code() const noexcept {
			return e.has_value() ? Errc::Ok : e.error().code;
		}

		constexpr decltype(auto) operator*() && { return *::std::move(e); }

		constexpr decltype(auto) operator*() & { return *e; }

		constexpr decltype(auto) operator*() const& { return *e; }

		constexpr auto operator->() { return &*e; }

		constexpr auto operator->() const { return &*e; }

		constexpr decltype(auto) orThrow() && {
			if (!e) throwError(e.error());
			if constexpr (!::std::is_void_v<T>) return *::std::move(e);
		}

		constexpr decltype(auto) orThrow() & {
			if (!e) throwError(e.error());
			if constexpr (!::std::is_void_v<T>) return *e;
		}
	};

	// Boundary between the internal layer (Errc) and the public one (result).
	[[nodiscard]] constexpr result<> asResult(Errc c, ::std::size_t pos = 0) noexcept {
		return c == Errc::Ok ? result<>{} : result<>{ c, pos };
	}

}  // namespace ser
