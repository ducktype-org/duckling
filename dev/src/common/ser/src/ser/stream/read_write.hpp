#pragma once

#include <base/except/exceptions.hpp>
#include <base/misc/no_unique_address.hpp>

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/internal/dispatch.hpp>
#include <ser/pool/context.hpp>
#include <ser/stream/header.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

namespace ser {

	/**
	 * @brief options
	 * @details What goes around the payload. The envelope is described in stream/header.hpp; this
	 * is the switch that puts one there.
	 *
	 * header DEFAULTS TO FALSE: without it a stream loses version, platform and schema
	 * validation and nothing else, and ser::write then produces exactly the payload - which
	 * is what makes the wire testable byte for byte.
	 *
	 * user_magic brands the stream as yours: four bytes after "SER\0" that a reader passes
	 * back in, so somebody else's ser stream is BadMagic rather than reaching your schema
	 * check. There is no checksum flag, because payload_crc is written as 0 and a flag that
	 * quietly does nothing is worse than an absent one.
	 */
	struct options final {
		bool            header     = false;
		::std::uint32_t user_magic = 0;
	};

	namespace internal {

		/**
		 * @brief The envelope on the throwing path. Every read funnels through readThrowing, and
		 * there a bad header is an exception rather than a code - the code paths get it
		 * back from the catch in ser::read. Reading it also POSITIONS the archive at the
		 * payload, so the caller just carries on.
		 */
		template<class T, class Ctx, class Ar>
		void readEnvelope(Ar& ar, const options& opt) {
			stream_header h{};
			if (const auto e = readHeader(ar, h, opt.user_magic); e != Errc::Ok)
				throwError(e, ar.position());
			if (const auto e = checkHeader<T, Ctx>(h); e != Errc::Ok) throwError(e, ar.position());
		}

	} /* namespace internal */

	/**
	 * @brief owned<T, Ctx>
	 * @details An AGGREGATE on purpose: ser::read builds it as
	 *     owned<T, Ctx>{ dispatchMake<T>(ar), std::move(ctx) }
	 * where the prvalue initializes `value` directly - no move, and it compiles for types
	 * with const fields.
	 */
	template<class T, class Ctx>
	struct owned final {
		T                     value;
		NO_UNIQUE_ADDRESS Ctx ctx;

		constexpr T* operator->() noexcept { return &value; }

		constexpr const T* operator->() const noexcept { return &value; }

		constexpr T& operator*() noexcept { return value; }

		constexpr const T& operator*() const noexcept { return value; }

		template<class P>
		constexpr P& pool() {
			return ctx.template pool<P>();
		}

		/**
		 * @brief Taking the object out is safe exactly when dropping the context is safe, which
		 * for now means an empty one. The && forces std::move(r).take(), so the call site
		 * says it consumes.
		 */
		[[nodiscard]] constexpr T take() && requires(::std::is_empty_v<Ctx>) {
			static_assert(
				::std::move_constructible<T>,
				"ser: owned::take needs a movable T - an object that cannot be moved "
				"cannot be relocated out of the bundle. Build it where it belongs: "
				"ser::in ar{bytes}; T obj{ ser::subMake<F>(ar), ... };"
			);
			return ::std::move(value);
		}
	};

	namespace internal {

		/**
		 * @brief how a panicking entry point stops
		 * @details The panicking forms are for bytes whose validity is an INVARIANT - an object we
		 * are holding on the way out, a buffer we grew ourselves - never for input. Anything off a
		 * disk, out of a socket or from an older build must go through ser::read instead:
		 * CORE_PANIC is std::unreachable() in Release, so a panicking form handed input that
		 * is allowed to be wrong is undefined behaviour there, with no diagnostic.
		 */
		[[noreturn]] inline void panicOnStreamError(const char* what, const error& e) {
			CORE_PANIC(what, e.message());
		}

		/** @brief the buffer has to be used up */
		template<class Ar>
		void requireFullyConsumed(Ar& ar) {
			if (ar.avail() != 0) throwError(Errc::TrailingBytes, ar.position());
		}

		/**
		 * @brief the throwing read, which never leaves this namespace
		 * @details An exception is the only way a read can both return the object BY VALUE and
		 * report an error: copy elision is then guaranteed end to end and T never has to be
		 * movable, while a return code has nowhere to sit next to the value.
		 *
		 * It is `internal` because that convention stops at the library's edge - every public
		 * entry point below turns the exception into a ser::result or a CORE_PANIC, so no
		 * ser::exception ever escapes into calling code.
		 */
		template<class T, class Ctx>
		[[nodiscard]] owned<T, Ctx> readThrowing(::std::span<const ::std::byte> bytes, options opt) {
			static_assert(
				!::std::is_array_v<T>,
				"ser: cannot return a C array by value. Read it in "
				"place: T arr; ser::in{bytes}(arr)."
			);
			Ctx     ctx{};
			in<Ctx> ar{ bytes, ctx };
			if (opt.header) readEnvelope<T, Ctx>(ar, opt);
			/*
			 * Left-to-right evaluation is guaranteed for braced init, so `ar` is done being
			 * used before `ctx` is moved out from under it - and that is also the one place
			 * the end-of-buffer check can run: after the object exists, without naming it,
			 * which would cost the move this path exists to avoid.
			 */
			return owned<T, Ctx>{ dispatchMake<T>(ar),
				                  (requireFullyConsumed(ar), ::std::move(ctx)) };
		}

		/**
		 * @brief The bare object, with no bundle around it. It drops the context, which is created
		 * here and destroyed on the way out, so anything in the object pointing into it would
		 * dangle before the caller saw it - hence the constraint, an EMPTY context having
		 * nothing to point into.
		 */
		template<class T, class Ctx>
		requires(::std::is_empty_v<Ctx>)
		[[nodiscard]] T readThrowingForce(::std::span<const ::std::byte> bytes, options opt) {
			static_assert(
				!::std::is_array_v<T>,
				"ser: cannot return a C array by value. Read it "
				"in place: T arr; ser::in{bytes}(arr)."
			);
			Ctx     ctx{};
			in<Ctx> ar{ bytes, ctx };
			if (opt.header) readEnvelope<T, Ctx>(ar, opt);

			if constexpr (::std::move_constructible<T>) {
				T value = dispatchMake<T>(ar);
				requireFullyConsumed(ar);
				return value;
			} else {
				return dispatchMake<T>(ar);
			}
		}

	} /* namespace internal */

	/*
	 * read
	 * Two ways to be told a stream is bad, and which one to reach for is a property of
	 * where the bytes came from:
	 *
	 *   read          -> result<owned<T, Ctx>>. The stream is INPUT: a file somebody else
	 *                    wrote, a cache from an older build, anything that is allowed to
	 *                    be wrong. A bad stream is information - log it and carry on
	 *                    without whatever it was going to give you.
	 *   readOrPanic   -> owned<T, Ctx>, and CORE_PANIC on a bad stream. The bytes are OURS
	 *                    - a blob this program wrote - so reading them back cannot fail
	 *                    unless the program is already broken, and there is no state to
	 *                    recover to. readOrPanicForce is the same thing returning a bare T.
	 *
	 * The line between them is not a matter of taste - see internal::panicOnStreamError. Input
	 * goes through ser::read. Always.
	 *
	 * Both return owned<T, Ctx> rather than a bare T, even when Ctx is empty, so that pools
	 * can be added later without touching a call site. readOrPanicForce is the exception,
	 * and it says so in its name.
	 */

	/**
	 * @brief The bundle reaches std::expected through a constructor parameter, and elision never
	 * crosses one - so this path costs exactly one move of owned<T, Ctx>.
	 */
	template<class T, class Ctx = no_context>
	[[nodiscard]] result<owned<T, Ctx>> read(::std::span<const ::std::byte> bytes, options opt = {}) {
		static_assert(
			::std::move_constructible<T>,
			"ser::read: T must be movable, because result<owned<T, Ctx>> has to "
			"move the bundle into std::expected. For a type that cannot be moved "
			"use ser::readOrPanic<T>(bytes) - it returns owned<T, Ctx> by value "
			"and elides everything."
		);
		try {
			return internal::readThrowing<T, Ctx>(bytes, opt);
		} catch (const exception& e) { return fail(e.err()); }
	}

	/**
	 * @brief `return f();` where f returns exactly this function's return type is still guaranteed
	 * copy elision inside a try block, so the object is built once, at its final address,
	 * and a T that cannot be moved reads fine here.
	 */
	template<class T, class Ctx = no_context>
	[[nodiscard]] owned<T, Ctx> readOrPanic(::std::span<const ::std::byte> bytes, options opt = {}) {
		try {
			return internal::readThrowing<T, Ctx>(bytes, opt);
		} catch (const exception& e) {
			internal::panicOnStreamError("failed to deserialize: ", e.err());
		}
	}

	/**
	 * @brief The bare object:
	 *
	 *     const Config cfg = ser::readOrPanicForce<Config>(bytes);
	 *
	 * A stateful context means readOrPanic instead, and the object stays bundled with the
	 * pools it may point into - see the note on internal::readThrowingForce.
	 */
	template<class T, class Ctx = no_context>
	requires(::std::is_empty_v<Ctx>)
	[[nodiscard]] T readOrPanicForce(::std::span<const ::std::byte> bytes, options opt = {}) {
		try {
			return internal::readThrowingForce<T, Ctx>(bytes, opt);
		} catch (const exception& e) {
			internal::panicOnStreamError("failed to deserialize: ", e.err());
		}
	}

	/**
	 * @brief write
	 * @details Appends. Into a growable buffer each call starts where the last one stopped, so
	 * several objects written separately end up as one stream. Into a fixed buffer there is
	 * nothing to append to - a span's size() is capacity - so each call fills it from the
	 * front.
	 *
	 *     ser::write(buf, header);         // buf: [header]
	 *     ser::write(buf, payload);        // buf: [header][payload]
	 *
	 * Reading them back is one ARCHIVE rather than two calls to ser::read, because ser::read
	 * always starts at the front of the span it is given and never says how far it got. An
	 * archive does say: ar.position() is public, and so are ar.size(), ar.avail() and
	 * ar.reset(p).
	 *
	 *     ser::in ar{bytes};
	 *     if (const auto e = ar(header); e != Errc::Ok) return {e, ar.position()};
	 *     if (const auto e = ar(payload); e != Errc::Ok) return {e, ar.position()};
	 *     const usize consumed = ar.position();   // where the next message begins
	 *
	 * One write is one call to finish(), which is what flushes that message's pools - so
	 * appending several messages means several self-contained messages, not one message in
	 * several pieces.
	 */
	template<class Buf, class T>
	[[nodiscard]] result<> write(Buf& buf, const T& x, options opt = {}) {
		out<Buf>            ar{ buf };
		const ::std::size_t start = ar.position();

		/**
		 * @brief Written twice, and that is what payload_size costs: the size is not known until the
		 * payload is out, so the first copy reserves the 32 bytes and the second - after
		 * finish(), so pool data counts as payload - patches them.
		 */
		stream_header h{};
		if (opt.header) {
			h = stream_header::forType<T>(opt.user_magic);
			if (const auto e = writeHeader(ar, h); e != Errc::Ok) return fail(e, ar.position());
		}
		const ::std::size_t payload_start = ar.position();

		if (const auto e = ar(x); e != Errc::Ok) return fail(e, ar.position());
		if (const auto e = ar.finish(); e != Errc::Ok) return fail(e, ar.position());

		if (opt.header) {
			const ::std::size_t end = ar.position();
			h.payload_size          = static_cast<::std::uint64_t>(end - payload_start);
			ar.reset(start);
			if (const auto e = writeHeader(ar, h); e != Errc::Ok) return fail(e, ar.position());
			ar.reset(end);
		}
		return ok();
	}

	/**
	 * @brief Read can legitimately fail because its input is somebody else's bytes,
	 * while a write is handed an object the caller is already holding
	 */
	template<class Buf, class T>
	void writeOrPanic(Buf& buf, const T& x, options opt = {}) {
		if (const auto r = write(buf, x, opt); !r)
			internal::panicOnStreamError("failed to serialize: ", r.error());
	}

} /* namespace ser */
