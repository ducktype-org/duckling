#pragma once

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>
#include <base/numeric/overflow.hpp>

#include <ser/archive/archive_base.hpp>
#include <ser/archive/buffer.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/internal/bytes.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/pool/context.hpp>

#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

	/**
	 * @brief position(), depth()/pushDepth()/popDepth(), pool<P>() and the context storage
	 * come from ArchiveBase - see the note there on why they are shared.
	 */
	template<class Buf, class Ctx = NoContext>
	class Out final: public internal::ArchiveBase<Ctx> {
		static_assert(
			ByteBuffer<Buf>,
			"ser::Out: Buf must expose data() -> std::byte* and size(). "
			"Use std::vector<std::byte> (growable) or std::span<std::byte> (fixed)."
		);

		using Base = internal::ArchiveBase<Ctx>;
		using Base::context;
		using Base::pos;  // the base is dependent, so pos is not found unqualified

		Buf& buf;

	public:
		static constexpr bool IS_WRITING = true;
		using BufferType                 = Buf;
		using ContextType                = Ctx;

		/**
		 * @brief where writing starts
		 * @details A growable buffer is APPENDED to - a fresh archive begins at buf.size() - while a
		 * fixed buffer is filled from the front, its size() being capacity and carrying no
		 * notion of how much is already written. To continue inside a fixed one, keep the
		 * archive and keep calling it.
		 *
		 * So position() is an offset into the BUFFER and not into the message, which is what
		 * makes an error position point at the byte that failed. reset() takes buffer
		 * offsets too.
		 */
		constexpr explicit Out(Buf& b) noexcept requires(::std::is_empty_v<Ctx>): buf(b) {
			pos = internal::bufferOrigin(b);
		}

		constexpr Out(Buf& b, Ctx& c) noexcept: Base(c), buf(b) { pos = internal::bufferOrigin(b); }

		/**
		 * @brief serialization entry point
		 * @details Stops at the first failure; the returned code belongs to that argument.
		 */
		template<class... Ts>
		constexpr Errc operator()(const Ts&... xs) {
			Errc e = Errc::Ok;
			(void) (((e = internal::dispatchWrite<Ts>(*this, xs)) == Errc::Ok) && ...);
			return e;
		}

		/**
		 * @brief raw byte access
		 * @details One copy straight into the buffer - no temporary, unlike rawWrite of a
		 * locally materialized byte array.
		 */
		template<class T>
		constexpr Errc writeRaw(const T& v) {
			if (const auto e = reserve(sizeof(T)); e != Errc::Ok) return e;
			internal::store(internal::bufferData(buf) + pos, v);
			pos += sizeof(T);
			return Errc::Ok;
		}

		constexpr Errc rawWrite(::std::span<const ::std::byte> src) {
			if (const auto e = reserve(src.size()); e != Errc::Ok) return e;
			internal::copyBytes(internal::bufferData(buf) + pos, src.data(), src.size());
			pos += src.size();
			return Errc::Ok;
		}

		/** @brief `n` is an increment on top of the current position, not an absolute size. */
		constexpr Errc reserve(::std::size_t n) {
			::std::size_t need = 0;
			if (::base::addOvf(pos, n, need)) return Errc::SizeOverflow;
			return internal::bufferEnsure(buf, need);
		}

		constexpr Errc padTo(::std::size_t align) {
			CORE_ASSERT(
				align != 0 && (align & (align - 1)) == 0,
				"ser::Out::padTo: alignment must be a power of two"
			);
			const ::std::size_t n = internal::alignUp(pos, align) - pos;
			if (n == 0) return Errc::Ok;
			if (const auto e = reserve(n); e != Errc::Ok) return e;
			internal::fillBytes(internal::bufferData(buf) + pos, ::std::byte{ 0 }, n);
			pos += n;
			return Errc::Ok;
		}

		/**
		 * @brief position
		 * @details Rewinding is how the header gets patched once the payload size is known. The
		 * bound is the buffer, which is why reset() is not shared with ser::In.
		 * Not noexcept: CORE_ASSERT throws base::Panic in a Dev build.
		 */
		constexpr void reset(::std::size_t p = 0) {
			CORE_ASSERT(
				p <= internal::bufferSize(buf),
				"ser::Out::reset: position past the end of the buffer"
			);
			pos = p;
		}

		/** @brief Always call it. Without pools it folds away to nothing. */
		constexpr Errc finish() { return context().finish(); }
	};

}  // namespace ser
