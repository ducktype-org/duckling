#pragma once

// ── the envelope ──────────────────────────────────────────────────────────────
// Thirty-two bytes in front of the payload: a stream from another version of the program,
// another byte order or another schema is refused with an error code instead of being
// interpreted as data.
//
//     [header 32 B]?  [payload]
//
// It is OPT-IN - ser::options{}.header is false - so a plain ser::write is exactly the
// payload and not one byte more. Thirty-two is a multiple of sixteen on purpose: the
// payload starts at a 16-byte boundary whenever the buffer does, so the envelope will not
// be in the way of zero-copy spans of types with alignof <= 16.
//
// The layout is frozen, has no padding (both asserted below) and every field is written in
// native byte order - `flags` is what makes that safe, because it is checked before
// anything else is believed.
//
//   char     magic[8]      "SER\0" + four bytes of user_magic, little-endian
//   u64      schema_hash   ser::schemaHash<T, Ctx>()
//   u64      payload_size  bytes after the header that belong to this message
//   u32      payload_crc   reserved, written as 0 - there is no checksum yet
//   u16      flags         ser::nativeFlags()
//   u16      header_size   32, and a reader trusts it rather than assuming
//
// header_size is read rather than assumed for one reason: a later version of the format
// may make the envelope longer, and a reader that skips to `header_size` can still read
// the payload of such a stream instead of refusing it. That is why readHeader positions
// the archive itself.

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/detail/ovf.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/pool/context.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace ser {

	struct stream_header {
		// A fixed eight-byte field of the wire layout, not a container.
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
		char            magic[8]     = { 'S', 'E', 'R', '\0', '\0', '\0', '\0', '\0' };
		::std::uint64_t schema_hash  = 0;
		::std::uint64_t payload_size = 0;
		::std::uint32_t payload_crc  = 0;
		::std::uint16_t flags        = 0;
		::std::uint16_t header_size  = 32;

		static constexpr ::std::size_t WIRE_SIZE = 32;

		// ── the magic ─────────────────────────────────────────────────────────
		// Four bytes that say "ser", four that say whose stream it is. user_magic is written
		// byte by byte, least significant first, rather than as a u32: a reader has to be
		// able to reject a foreign stream BEFORE it trusts `flags`, so the magic cannot
		// depend on the byte order.
		[[nodiscard]] constexpr bool magicOk() const noexcept {
			return magic[0] == 'S' && magic[1] == 'E' && magic[2] == 'R' && magic[3] == '\0';
		}

		constexpr void setUserMagic(::std::uint32_t m) noexcept {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
			for (int i = 0; i < 4; ++i) magic[4 + i] = static_cast<char>((m >> (8 * i)) & 0xFFu);
		}

		[[nodiscard]] constexpr ::std::uint32_t userMagic() const noexcept {
			::std::uint32_t m = 0;
			for (int i = 0; i < 4; ++i)
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				m |= static_cast<::std::uint32_t>(static_cast<unsigned char>(magic[4 + i]))
				  << (8 * i);
			return m;
		}

		// The whole message: this header plus its payload. A caller that appends several
		// messages into one buffer uses it to find the next one.
		[[nodiscard]] constexpr ::std::uint64_t totalSize() const noexcept {
			return static_cast<::std::uint64_t>(header_size) + payload_size;
		}

		// payload_size is filled in after the payload is written - see the patch in
		// ser::write - so a header handed to writeHeader may carry a zero here.
		template<class T, class Ctx = no_context>
		[[nodiscard]] static constexpr stream_header forType(::std::uint32_t user = 0) noexcept {
			stream_header h{};
			h.setUserMagic(user);
			h.schema_hash = ::ser::schemaHash<T, Ctx>();
			h.flags       = ::ser::nativeFlags();
			h.header_size = static_cast<::std::uint16_t>(WIRE_SIZE);
			return h;
		}

		friend constexpr bool operator==(const stream_header&, const stream_header&) = default;
	};

	static_assert(
		sizeof(stream_header) == stream_header::WIRE_SIZE,
		"ser: the envelope is a fixed 32 bytes - see the layout note above."
	);
	static_assert(
		::std::has_unique_object_representations_v<stream_header>,
		"ser: the envelope must have no padding, because it is written as its "
		"own object representation and padding would put indeterminate bytes "
		"into the stream."
	);
	static_assert(::std::is_trivially_copyable_v<stream_header>);

	// One store of 32 bytes, wherever the archive currently is - so a caller can put an
	// envelope in front of each of several messages in one buffer.
	template<writer Ar>
	constexpr Errc writeHeader(Ar& ar, const stream_header& h) {
		return ar.writeRaw(h);
	}

	// ── reading it ────────────────────────────────────────────────────────────
	// Everything that does not need the type, in a fixed order, because the order is the
	// diagnosis: a stream from the other byte order is not a stream with a different
	// schema, and saying "SchemaMismatch" for it would send the reader looking in the
	// wrong place.
	//
	//   1. 32 bytes are there                      -> Truncated
	//   2. magic, user_magic, header_size >= 32    -> BadMagic
	//   3. flags == nativeFlags()                 -> PlatformMismatch
	//   4. header_size + payload_size fits         -> SizeOverflow / Truncated
	//
	// On success the archive is positioned at the payload - past header_size bytes, not
	// past 32, so a longer envelope from a later version is skipped rather than misread.
	// The schema is step 5 and lives in checkHeader, because that one needs the type.
	template<reader Ar>
	constexpr Errc readHeader(Ar& ar, stream_header& h, ::std::uint32_t user_magic = 0) {
		const ::std::size_t start = ar.position();

		stream_header raw{};
		if (const auto e = ar.readRaw(raw); e != Errc::Ok) return e;

		if (!raw.magicOk()) return Errc::BadMagic;
		if (raw.userMagic() != user_magic) return Errc::BadMagic;
		if (raw.header_size < stream_header::WIRE_SIZE) return Errc::BadMagic;
		if (raw.flags != nativeFlags()) return Errc::PlatformMismatch;

		// In u64 throughout, never in size_t: payload_size comes off the wire and a
		// 32-bit reader must not turn 2^32 + 4 into 4 by narrowing it.
		::std::uint64_t total = 0;
		if (detail::addOvf(static_cast<::std::uint64_t>(start), raw.totalSize(), total))
			return Errc::SizeOverflow;
		if (total > static_cast<::std::uint64_t>(ar.size())) return Errc::Truncated;

		ar.reset(start + raw.header_size);
		h = raw;
		return Errc::Ok;
	}

	// The part that needs the type. Split out so that a caller reading several different
	// messages out of one stream can look at the header first and decide what to read.
	template<class T, class Ctx = no_context>
	[[nodiscard]] constexpr Errc checkHeader(const stream_header& h) noexcept {
		return h.schema_hash == ::ser::schemaHash<T, Ctx>() ? Errc::Ok : Errc::SchemaMismatch;
	}

	// What is in front of this buffer, whatever type it turns out to describe. The
	// schema_hash comes back as data, so a caller holding several possible types can
	// compare it against ser::schemaHash<T>() for each of them and pick.
	[[nodiscard]] inline result<stream_header> peekHeader(
		::std::span<const ::std::byte> bytes, ::std::uint32_t user_magic = 0
	) {
		in<no_context> ar{ bytes };
		stream_header  h{};
		if (const auto e = readHeader(ar, h, user_magic); e != Errc::Ok)
			return result<stream_header>{ e, ar.position() };
		return result<stream_header>{ h };
	}

}  // namespace ser
