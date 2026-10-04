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

	/**
	 * @brief Why a write or a read stopped. `Ok` is the only success; toString() gives the
	 * message for each code.
	 */
	enum class Errc : ::std::uint8_t {
		Ok = 0,

		// the buffer: too short to read from, too small to write into, or not used up
		Truncated,
		BufferFull,
		UnexpectedEnd,
		TrailingBytes,

		// the values: a length or a value the stream cannot hold, or nesting too deep
		SizeOverflow,
		MessageSize,
		InvalidValue,
		Misaligned,
		DepthExceeded,

		// the header: not a ser stream, or written for another type or platform
		BadMagic,
		SchemaMismatch,
		PlatformMismatch,
		ChecksumFailed,

		// the pools: nothing returns these yet, see #90002 and #90003
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
		case Errc::TrailingBytes:
			return "bytes left over after the object";
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

	struct Error final {
		Errc          code     = Errc::Ok;
		::std::size_t position = 0;

		[[nodiscard]] ::std::string message() const {
			return ::std::string{ toString(code) } + " (at position " + ::std::to_string(position)
			     + ")";
		}

		friend constexpr bool operator==(const Error&, const Error&) = default;
	};

	class Exception final: public ::std::runtime_error {
		Error e;

	public:
		explicit Exception(Error err): ::std::runtime_error(err.message()), e(err) {}

		[[nodiscard]] const Error& err() const noexcept { return e; }

		[[nodiscard]] Errc code() const noexcept { return e.code; }
	};

	[[noreturn]] inline void throwError(Error e) { throw Exception{ e }; }

	[[noreturn]] inline void throwError(Errc c, ::std::size_t pos = 0) {
		throwError(Error{ .code = c, .position = pos });
	}

	/**
	 * @brief What every public entry point hands back: the value, or the `Error` that stopped it.
	 * @details A plain alias for `std::expected`, so the standard vocabulary applies -
	 * `has_value()`, `error()`, `operator*`, `value_or` and the monadic `and_then` / `transform`.
	 * The helpers below are the whole reason there is no wrapper class: they build the error side
	 * straight from an `Errc`, so no call site has to spell `std::unexpected` by hand.
	 * @note Every function returning one is `[[nodiscard]]` in its own right, since the alias
	 * cannot carry the attribute the way a class could.
	 */
	template<class T = void>
	using Result = ::std::expected<T, Error>;

	/** @brief The error side of a `Result`, from a code and the position that failed. */
	[[nodiscard]] constexpr ::std::unexpected<Error> fail(Errc c, ::std::size_t pos = 0) noexcept {
		return ::std::unexpected(Error{ .code = c, .position = pos });
	}

	/** @brief The error side of a `Result`, from an `Error` that has already been formed. */
	[[nodiscard]] constexpr ::std::unexpected<Error> fail(Error e) noexcept {
		return ::std::unexpected(e);
	}

	/** @brief A successful `Result<>`, for a call that produces no value. */
	[[nodiscard]] constexpr Result<> ok() noexcept { return Result<>{}; }

	/** @brief Boundary between the internal layer (`Errc`) and the public one (`Result`). */
	[[nodiscard]] constexpr Result<> asResult(Errc c, ::std::size_t pos = 0) noexcept {
		return c == Errc::Ok ? ok() : Result<>{ fail(c, pos) };
	}

	/** @brief The code a `Result` carries, or `Errc::Ok` when it holds a value. */
	template<class T>
	[[nodiscard]] constexpr Errc codeOf(const Result<T>& r) noexcept {
		return r.has_value() ? Errc::Ok : r.error().code;
	}

	/**
	 * @brief The value of a `Result`, or a thrown `ser::Exception` when it holds an error.
	 * @details Free rather than a member, so `Result` stays a plain `std::expected` with no
	 * throwing surface of its own.
	 */
	template<class T>
	constexpr decltype(auto) orThrow(Result<T>&& r) {
		if (!r) throwError(r.error());
		if constexpr (!::std::is_void_v<T>) return *::std::move(r);
	}

	/** @brief The value of a `Result`, or a thrown `ser::Exception` when it holds an error. */
	template<class T>
	constexpr decltype(auto) orThrow(Result<T>& r) {
		if (!r) throwError(r.error());
		if constexpr (!::std::is_void_v<T>) return *r;
	}

} /* namespace ser */
