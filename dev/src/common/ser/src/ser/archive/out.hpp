#pragma once

#include <ser/archive/archive_base.hpp>
#include <ser/archive/buffer.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/debug.hpp>
#include <ser/detail/bytes.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/meta.hpp>
#include <ser/detail/ovf.hpp>
#include <ser/errc.hpp>
#include <ser/pool/context.hpp>

#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

	// position(), depth()/pushDepth()/popDepth(), pool<P>() and the context storage
	// come from archive_base - see the note there on why they are shared.
	template<class Buf, class Ctx = no_context>
	class out: public detail::archive_base<Ctx> {
		static_assert(
			byte_buffer<Buf>,
			"ser::out: Buf must expose data() -> std::byte* and size(). "
			"Use std::vector<std::byte> (growable) or std::span<std::byte> (fixed)."
		);

		using base = detail::archive_base<Ctx>;
		using base::context;
		using base::pos;  // the base is dependent, so pos is not found unqualified

		Buf& buf;

	public:
		static constexpr bool IS_WRITING = true;
		using buffer_type                = Buf;
		using context_type               = Ctx;

		// ── where writing starts ──────────────────────────────────────────────
		// A growable buffer is APPENDED to: a fresh archive begins at buf.size(), so
		// several independent writes accumulate into one stream and nothing already in
		// the buffer is overwritten. A fixed buffer is filled from the front, because
		// its size() is capacity and carries no notion of how much is already written -
		// to continue inside one, keep the archive and keep calling it.
		//
		// The consequence worth knowing: position() is an offset into the BUFFER, not
		// into the message, so an error position points at the byte that failed. reset()
		// takes buffer offsets too.
		constexpr explicit out(Buf& b) noexcept requires(::std::is_empty_v<Ctx>): buf(b) {
			pos = detail::bufferOrigin(b);
		}

		constexpr out(Buf& b, Ctx& c) noexcept: base(c), buf(b) { pos = detail::bufferOrigin(b); }

		// ── serialization entry point ─────────────────────────────────────────
		// Stops at the first failure; the returned code belongs to that argument.
		template<class... Ts>
		constexpr Errc operator()(const Ts&... xs) {
			Errc e = Errc::Ok;
			(void) (((e = detail::dispatchWrite<Ts>(*this, xs)) == Errc::Ok) && ...);
			return e;
		}

		// ── raw byte access ───────────────────────────────────────────────────
		// One copy straight into the buffer - no temporary, unlike rawWrite of a
		// locally materialized byte array.
		template<class T>
		constexpr Errc writeRaw(const T& v) {
			if (const auto e = reserve(sizeof(T)); e != Errc::Ok) return e;
			detail::store(detail::bufferData(buf) + pos, v);
			pos += sizeof(T);
			return Errc::Ok;
		}

		constexpr Errc rawWrite(::std::span<const ::std::byte> src) {
			if (const auto e = reserve(src.size()); e != Errc::Ok) return e;
			detail::copyBytes(detail::bufferData(buf) + pos, src.data(), src.size());
			pos += src.size();
			return Errc::Ok;
		}

		// `n` is an increment on top of the current position, not an absolute size.
		constexpr Errc reserve(::std::size_t n) {
			::std::size_t need = 0;
			if (detail::addOvf(pos, n, need)) return Errc::SizeOverflow;
			return detail::bufferEnsure(buf, need);
		}

		constexpr Errc padTo(::std::size_t align) {
			SER_ASSERT(
				align != 0 && (align & (align - 1)) == 0,
				"ser::out::padTo: alignment must be a power of two"
			);
			const ::std::size_t n = detail::alignUp(pos, align) - pos;
			if (n == 0) return Errc::Ok;
			if (const auto e = reserve(n); e != Errc::Ok) return e;
			detail::fillBytes(detail::bufferData(buf) + pos, ::std::byte{ 0 }, n);
			pos += n;
			return Errc::Ok;
		}

		// ── position ──────────────────────────────────────────────────────────
		// Rewinding is how the header gets patched once the payload size is known. The
		// bound is the buffer, which is why reset() is not shared with ser::in.
		constexpr void reset(::std::size_t p = 0) noexcept {
			SER_ASSERT(
				p <= detail::bufferSize(buf), "ser::out::reset: position past the end of the buffer"
			);
			pos = p;
		}

		// Always call it. Without pools it folds away to nothing.
		constexpr Errc finish() { return context().finish(); }
	};

}  // namespace ser
