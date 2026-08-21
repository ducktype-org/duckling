#pragma once

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/detail/dispatch.hpp>
#include <ser/errc.hpp>
#include <ser/pool/context.hpp>
#include <ser/stream/header.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

namespace ser {

	// ── options ───────────────────────────────────────────────────────────────
	// What goes around the payload. The envelope is described in stream/header.hpp; this
	// is the switch that puts one there.
	//
	// header DEFAULTS TO FALSE: without it a stream loses version, platform and schema
	// validation and nothing else, and ser::write then produces exactly the payload -
	// buf.size() is the bytes the object needed, which is what makes the wire testable
	// byte for byte.
	//
	// user_magic brands the stream as yours: four bytes after "SER\0" that a reader passes
	// back in, so somebody else's ser stream is rejected as BadMagic rather than reaching
	// your schema check.
	//
	// There is no checksum flag: the header carries payload_crc and writes 0 into it, and
	// a flag that quietly does nothing is worse than an absent one.
	struct options {
		bool            header     = false;
		::std::uint32_t user_magic = 0;
	};

	namespace detail {

		// The envelope on the throwing path. Every read funnels through readOrThrow, and
		// there a bad header is an exception rather than a code - the code paths get it
		// back from the catch in ser::read. Reading it also POSITIONS the archive at the
		// payload, so the caller just carries on.
		template<class T, class Ctx, class Ar>
		void readEnvelope(Ar& ar, const options& opt) {
			stream_header h{};
			if (const auto e = readHeader(ar, h, opt.user_magic); e != Errc::Ok)
				throwError(e, ar.position());
			if (const auto e = checkHeader<T, Ctx>(h); e != Errc::Ok) throwError(e, ar.position());
		}

	}  // namespace detail

	// ── owned<T, Ctx> ─────────────────────────────────────────────────────────
	// An AGGREGATE on purpose. ser::read builds it as
	//     owned<T, Ctx>{ dispatchMake<T>(ar), std::move(ctx) }
	// where the prvalue initializes `value` directly - no move, and it compiles for types
	// with const fields. `owned r; r.value = ...` would throw away the copy elision the
	// whole make path exists for.
	//
	// Field order matters: `value` is declared first, so it is destroyed first. Once the
	// object can refer into the pools held by `ctx`, it must die before them.
	template<class T, class Ctx>
	struct owned {
		T                         value;
		SER_NO_UNIQUE_ADDRESS Ctx ctx;

		constexpr T* operator->() noexcept { return &value; }

		constexpr const T* operator->() const noexcept { return &value; }

		constexpr T& operator*() noexcept { return value; }

		constexpr const T& operator*() const noexcept { return value; }

		template<class P>
		constexpr P& pool() {
			return ctx.template pool<P>();
		}

		// Taking the object out is safe exactly when dropping the context is safe, which
		// for now means an empty one. The && forces std::move(r).take(), so the call site
		// says it consumes.
		[[nodiscard]] constexpr T take() && requires(::std::is_empty_v<Ctx>) {
			static_assert(
				::std::move_constructible<T>,
				"ser: owned::take needs a movable T - an object that cannot be moved "
				"cannot be relocated out of the bundle. Build it where it belongs: "
				"ser::in ar{bytes}; T obj{ ser::readField<F>(ar), ... };"
			);
			return ::std::move(value);
		}
	};

	// ── read ──────────────────────────────────────────────────────────────────
	// Both entry points return owned<T, Ctx>, never a bare T - even when Ctx is empty, so
	// that pools can be added without touching a single call site.

	// Errors travel as an exception, which buys the one thing ser::read cannot have:
	// the return type equals the type of the returned prvalue, so copy elision is
	// guaranteed end to end and T never has to be movable.
	template<class T, class Ctx = no_context>
	[[nodiscard]] owned<T, Ctx> readOrThrow(::std::span<const ::std::byte> bytes, options opt = {}) {
		static_assert(
			!::std::is_array_v<T>,
			"ser::readOrThrow: cannot return a C array by value. Read it in "
			"place: T arr; ser::in{bytes}(arr)."
		);
		Ctx     ctx{};
		in<Ctx> ar{ bytes, ctx };
		if (opt.header) detail::readEnvelope<T, Ctx>(ar, opt);
		// Left-to-right evaluation is guaranteed for braced init, so `ar` is done being
		// used before `ctx` is moved out from under it.
		return owned<T, Ctx>{ detail::dispatchMake<T>(ar), ::std::move(ctx) };
	}

	// ── readOrThrowForce ───────────────────────────────────────────────────
	// The bare object, with no bundle around it. Same construction guarantee as
	// readOrThrow - the prvalue out of dispatchMake initializes the caller's object
	// directly, so a type that cannot be moved works here too - and none of the ceremony:
	//
	//     const Config cfg = ser::readOrThrowForce<Config>(bytes);
	//
	// What it drops is the context, which is created here and destroyed on the way out: so
	// anything in the object that points into it would be dangling before the caller sees
	// it, and that is undefined behaviour rather than an error code. Hence the constraint -
	// an EMPTY context has nothing to point into. A stateful one means readOrThrow, and the
	// object stays bundled with the pools it may point into.
	template<class T, class Ctx = no_context>
	requires(::std::is_empty_v<Ctx>)
	[[nodiscard]] T readOrThrowForce(::std::span<const ::std::byte> bytes, options opt = {}) {
		static_assert(
			!::std::is_array_v<T>,
			"ser::readOrThrowForce: cannot return a C array by value. Read it "
			"in place: T arr; ser::in{bytes}(arr)."
		);
		Ctx     ctx{};
		in<Ctx> ar{ bytes, ctx };
		if (opt.header) detail::readEnvelope<T, Ctx>(ar, opt);
		return detail::dispatchMake<T>(ar);
	}

	// The bundle reaches std::expected through a constructor parameter, and elision never
	// crosses one - so this path costs exactly one move of owned<T, Ctx>.
	template<class T, class Ctx = no_context>
	[[nodiscard]] result<owned<T, Ctx>> read(::std::span<const ::std::byte> bytes, options opt = {}) {
		static_assert(
			::std::move_constructible<T>,
			"ser::read: T must be movable, because result<owned<T, Ctx>> has to "
			"move the bundle into std::expected. For a type that cannot be moved "
			"use ser::readOrThrow<T>(bytes) - it returns owned<T, Ctx> by value "
			"and elides everything."
		);
		try {
			return readOrThrow<T, Ctx>(bytes, opt);
		} catch (const exception& e) { return result<owned<T, Ctx>>{ e.err() }; }
	}

	// ── write ─────────────────────────────────────────────────────────────────
	// Appends. Into a growable buffer each call starts where the last one stopped, so
	// several objects written separately end up as one stream and buf.size() is always
	// exactly the bytes produced so far - nothing is trimmed and nothing stale is left
	// behind. Into a fixed buffer there is nothing to append to (a span's size() is
	// capacity), so each call fills it from the front.
	//
	//     ser::write(buf, header);         // buf: [header]
	//     ser::write(buf, payload);        // buf: [header][payload]
	//
	// Reading them back is the mirror image, and it is one archive rather than two
	// calls, because ser::read starts at the front of the span it is given and does not
	// report how far it got:
	//
	//     ser::in ar{bytes};
	//     ar(header, payload);
	//
	// One write is one call to finish(), which is what flushes that message's pools - so
	// appending several messages means several self-contained messages, not one message in
	// several pieces.
	template<class Buf, class T>
	[[nodiscard]] result<> write(Buf& buf, const T& x, options opt = {}) {
		out<Buf>            ar{ buf };
		const ::std::size_t start = ar.position();

		// Written twice when there is one, and that is what payload_size costs: the size
		// is not known until the payload is out, so the first copy reserves the 32 bytes
		// and the second one - after finish(), so that pool data counts as payload -
		// patches them. ser::out::reset exists for exactly this.
		stream_header h{};
		if (opt.header) {
			h = stream_header::forType<T>(opt.user_magic);
			if (const auto e = writeHeader(ar, h); e != Errc::Ok)
				return result<>{ e, ar.position() };
		}
		const ::std::size_t payload_start = ar.position();

		if (const auto e = ar(x); e != Errc::Ok) return result<>{ e, ar.position() };
		if (const auto e = ar.finish(); e != Errc::Ok) return result<>{ e, ar.position() };

		if (opt.header) {
			const ::std::size_t end = ar.position();
			h.payload_size          = static_cast<::std::uint64_t>(end - payload_start);
			ar.reset(start);
			if (const auto e = writeHeader(ar, h); e != Errc::Ok)
				return result<>{ e, ar.position() };
			ar.reset(end);
		}
		return result<>{};
	}

}  // namespace ser
