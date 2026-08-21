#pragma once

// ── the envelope ──────────────────────────────────────────────────────────────
// Thirty-two bytes in front of the payload, and what they buy is the difference between
// a library and a toy: a stream from another version of the program, another byte order
// or another schema is refused with an error code instead of being interpreted as data.
//
//     [header 32 B]?  [payload]
//
// It is OPT-IN in M1 - ser::options{}.header is false - so a plain ser::write is still
// exactly the payload and not one byte more. That default flips when pools arrive:
// without a header there is no schema_hash, and without a schema_hash there is nothing
// that can notice the reader is using a different Ctx than the writer (CLAUDE.md, the
// M2 completeness check).
//
// Thirty-two is a multiple of sixteen on purpose. The payload therefore starts at a
// 16-byte boundary whenever the buffer does, which is what makes zero-copy spans of
// types with alignof <= 16 possible later without the envelope being in the way.
//
// The layout is frozen, has no padding (both asserted below) and every field is written
// in native byte order - `flags` is what makes that safe, because it is checked before
// anything else is believed.
//
//   char     magic[8]      "SER\0" + four bytes of user_magic, little-endian
//   u64      schema_hash   ser::schema_hash<T, Ctx>()
//   u64      payload_size  bytes after the header that belong to this message
//   u32      payload_crc   0 in M1 - CRC32C arrives with options.checksum in M2
//   u16      flags         ser::native_flags()
//   u16      header_size   32, and a reader trusts it rather than assuming
//
// header_size is read rather than assumed for one reason: a later version of the format
// may make the envelope longer, and a reader that skips to `header_size` can still read
// the payload of such a stream instead of refusing it. That is why read_header positions
// the archive itself.

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/pool/context.hpp>

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/detail/ovf.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace ser {

    struct stream_header {
        char            magic[8]     = {'S', 'E', 'R', '\0', '\0', '\0', '\0', '\0'};
        ::std::uint64_t schema_hash  = 0;
        ::std::uint64_t payload_size = 0;
        ::std::uint32_t payload_crc  = 0;
        ::std::uint16_t flags        = 0;
        ::std::uint16_t header_size  = 32;

        static constexpr ::std::size_t wire_size = 32;

        // ── the magic ─────────────────────────────────────────────────────────
        // Four bytes that say "ser", four that say whose stream it is. user_magic is the
        // caller's, and it is written byte by byte, least significant first, rather than
        // as a u32 - a reader has to be able to reject a foreign stream BEFORE it trusts
        // `flags`, and that means the magic cannot depend on the byte order.
        [[nodiscard]] constexpr bool magic_ok() const noexcept {
            return magic[0] == 'S' && magic[1] == 'E' && magic[2] == 'R' && magic[3] == '\0';
        }

        constexpr void set_user_magic(::std::uint32_t m) noexcept {
            for (int i = 0; i < 4; ++i)
                magic[4 + i] = static_cast<char>((m >> (8 * i)) & 0xFFu);
        }

        [[nodiscard]] constexpr ::std::uint32_t user_magic() const noexcept {
            ::std::uint32_t m = 0;
            for (int i = 0; i < 4; ++i)
                m |= static_cast<::std::uint32_t>(static_cast<unsigned char>(magic[4 + i]))
                     << (8 * i);
            return m;
        }

        // The whole message: this header plus its payload. A caller that appends several
        // messages into one buffer uses it to find the next one.
        [[nodiscard]] constexpr ::std::uint64_t total_size() const noexcept {
            return static_cast<::std::uint64_t>(header_size) + payload_size;
        }

        // payload_size is filled in after the payload is written - see the patch in
        // ser::write - so a header handed to write_header may carry a zero here.
        template <class T, class Ctx = no_context>
        [[nodiscard]] static constexpr stream_header for_type(::std::uint32_t user = 0) noexcept {
            stream_header h{};
            h.set_user_magic(user);
            h.schema_hash = ::ser::schema_hash<T, Ctx>();
            h.flags       = ::ser::native_flags();
            h.header_size = static_cast<::std::uint16_t>(wire_size);
            return h;
        }

        friend constexpr bool operator==(const stream_header&, const stream_header&) = default;
    };

    static_assert(sizeof(stream_header) == stream_header::wire_size,
                  "ser: the envelope is a fixed 32 bytes - see the layout note above.");
    static_assert(::std::has_unique_object_representations_v<stream_header>,
                  "ser: the envelope must have no padding, because it is written as its "
                  "own object representation and padding would put indeterminate bytes "
                  "into the stream.");
    static_assert(::std::is_trivially_copyable_v<stream_header>);

    // ── writing it ────────────────────────────────────────────────────────────
    // One store of 32 bytes, which is exactly what the layout above promises. It writes
    // wherever the archive currently is, so a caller can put an envelope in front of each
    // of several messages in one buffer.
    template <writer Ar>
    constexpr errc write_header(Ar& ar, const stream_header& h) {
        return ar.write_raw(h);
    }

    // ── reading it ────────────────────────────────────────────────────────────
    // Everything that does not need the type, in the order the design fixes, because the
    // order is the diagnosis: a stream from the other byte order is not a stream with a
    // different schema, and saying "schema_mismatch" for it would send the reader looking
    // in the wrong place.
    //
    //   1. 32 bytes are there                      -> truncated
    //   2. magic, user_magic, header_size >= 32    -> bad_magic
    //   3. flags == native_flags()                 -> platform_mismatch
    //   4. header_size + payload_size fits         -> size_overflow / truncated
    //
    // On success the archive is positioned at the payload - past header_size bytes, not
    // past 32, so a longer envelope from a later version is skipped rather than misread.
    // The schema is step 5 and lives in check_header, because that one needs the type.
    template <reader Ar>
    constexpr errc read_header(Ar& ar, stream_header& h, ::std::uint32_t user_magic = 0) {
        const ::std::size_t start = ar.position();

        stream_header raw{};
        if (const auto e = ar.read_raw(raw); e != errc::ok) return e;

        if (!raw.magic_ok())                            return errc::bad_magic;
        if (raw.user_magic() != user_magic)             return errc::bad_magic;
        if (raw.header_size < stream_header::wire_size) return errc::bad_magic;
        if (raw.flags != native_flags())                return errc::platform_mismatch;

        // In u64 throughout, never in size_t: payload_size comes off the wire and a
        // 32-bit reader must not turn 2^32 + 4 into 4 by narrowing it.
        ::std::uint64_t total = 0;
        if (detail::add_ovf(static_cast<::std::uint64_t>(start), raw.total_size(), total))
            return errc::size_overflow;
        if (total > static_cast<::std::uint64_t>(ar.size())) return errc::truncated;

        ar.reset(start + raw.header_size);
        h = raw;
        return errc::ok;
    }

    // The part that needs the type. Split out so that a caller reading several different
    // messages out of one stream can look at the header first and decide what to read.
    template <class T, class Ctx = no_context>
    [[nodiscard]] constexpr errc check_header(const stream_header& h) noexcept {
        return h.schema_hash == ::ser::schema_hash<T, Ctx>() ? errc::ok : errc::schema_mismatch;
    }

    // ── looking without reading ───────────────────────────────────────────────
    // What is in front of this buffer, whatever type it turns out to describe. The
    // schema_hash comes back as data, so a caller holding several possible types can
    // compare it against ser::schema_hash<T>() for each of them and pick.
    [[nodiscard]] inline result<stream_header> peek_header(::std::span<const ::std::byte> bytes,
                                                          ::std::uint32_t user_magic = 0) {
        in<no_context> ar{bytes};
        stream_header  h{};
        if (const auto e = read_header(ar, h, user_magic); e != errc::ok)
            return result<stream_header>{e, ar.position()};
        return result<stream_header>{h};
    }

} // namespace ser
