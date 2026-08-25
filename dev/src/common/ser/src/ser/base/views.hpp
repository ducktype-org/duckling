#pragma once

// ── base::RawView, ModRawView, OwningView, SharedView ─────────────────────────
// The split is ownership again, and here it is the same line the library already draws for
// std::string_view (see the note in std/string.hpp): a view that does not own its bytes
// cannot be read back, because the only thing a read could do is point into the input
// buffer, which stops being valid the moment the caller frees it.
//
//   RawView / ModRawView   refused. Non-owning.
//   OwningView             length prefix, then the bytes. Round-trips to a fresh
//                          allocation of its own.
//   SharedView             the same bytes. It cannot be filled in place - its content is
//                          immutable and behind a SharedBox - so a read replaces the whole
//                          view, and two SharedViews read from one stream do NOT end up
//                          sharing (see below).
//
// Both owning views hash the same, because the format is the same: a length and that many
// bytes. So an OwningView stream reads into a SharedView and back.
//
// What is NOT preserved is SharedView's sharing: two SharedViews onto one buffer each write
// their own copy and each read back into their own allocation. For immutable bytes that is
// a memory cost and not a change of meaning, which is why this is allowed while
// SharedBox<T> - where sharing a MUTABLE object is the point - is refused.

#include <base/misc/raw_view.hpp>
#include <base/misc/shared_view.hpp>

#include <ser/concepts.hpp>
#include <ser/detail/bytes.hpp>
#include <ser/detail/container.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <cstdint>

namespace ser {

	namespace detail {

		// Keyed to the archive, not to the view type: these are FULL specializations, so an
		// assert naming the view would fire the moment the header is parsed. See the note on
		// denyNonOwningRef in refs.hpp.
		template<class Ar>
		constexpr Errc denyNonOwningView() {
			static_assert(
				DEPENDENT_FALSE<Ar>,
				"ser: cannot serialize base::RawView / base::ModRawView - it does not own its "
				"bytes, so reading one could only make it point into the input buffer, which "
				"the caller is free to destroy the moment the read returns. Writing one and "
				"never reading it back is refused by the library's own pairing rule, and "
				"correctly.\n"
				"  Owns the bytes?        base::OwningView or base::SharedView "
				"(<ser/base/views.hpp>)\n"
				"  Text?                  std::string (<ser/std/string.hpp>)\n"
				"  A window into a payload you serialize anyway?  store the offset and the "
				"length as integers and rebuild the view after reading"
			);
			return Errc::InvalidValue;
		}

		// Length prefix, then the bytes in ONE copy. A std::byte has no representation to
		// swap and no value to validate, so a dispatch loop would produce the same stream and
		// buy nothing. Still constexpr-safe: detail::copyBytes switches to an element loop
		// under `if consteval`.
		constexpr Errc writeByteBlob(writer auto& ar, const ::base::RawView& v) {
			if (const auto c = writeLength(ar, v.size()); c != Errc::Ok) return c;
			return ar.rawWrite({ v.getBegin(), v.size() });
		}

		// Reads the prefix, allocates exactly that much and fills it. The buffer is handed out
		// raw because both owning views take ownership of a `new byte[]` block.
		constexpr Errc readByteBlob(reader auto& ar, ::std::byte*& out, ::std::size_t& out_size) {
			::std::size_t n = 0;
			// readLength is what makes the allocation below safe - it refuses a prefix
			// claiming more bytes than the stream can hold. ensure() after it is take()'s
			// precondition.
			if (const auto c = readLength<::std::byte>(ar, n); c != Errc::Ok) return c;
			if (const auto c = ar.ensure(n); c != Errc::Ok) return c;

			auto* raw = new ::std::byte[n];
			copyBytes(raw, ar.take(n).data(), n);

			out      = raw;
			out_size = n;
			return Errc::Ok;
		}

	}  // namespace detail

	// ── the two refusals ──────────────────────────────────────────────────────

	template<>
	struct serializer<::base::RawView> {
		static constexpr Errc write(writer auto& ar, const ::base::RawView&) {
			return detail::denyNonOwningView<decltype(ar)>();
		}

		static constexpr Errc read(reader auto& ar, ::base::RawView&) {
			return detail::denyNonOwningView<decltype(ar)>();
		}
	};

	template<>
	struct serializer<::base::ModRawView> {
		static constexpr Errc write(writer auto& ar, const ::base::ModRawView&) {
			return detail::denyNonOwningView<decltype(ar)>();
		}

		static constexpr Errc read(reader auto& ar, ::base::ModRawView&) {
			return detail::denyNonOwningView<decltype(ar)>();
		}
	};

	// ── OwningView ────────────────────────────────────────────────────────────

	template<>
	struct min_wire_size<::base::OwningView> {
		static constexpr ::std::size_t VALUE = sizeof(detail::wire_size_type);
	};

	// Shared with SharedView on purpose - see the note at the top.
	namespace detail {

		template<class Mode, class Seen>
		[[nodiscard]] consteval ::std::uint64_t schemaByteBlob(::std::uint64_t h) {
			return schemaOf<::std::byte, Mode, Seen>(schemaText(h, "base.byte_blob"));
		}

	}  // namespace detail

	template<>
	struct schema<::base::OwningView> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaByteBlob<Mode, Seen>(h);
		}
	};

	template<>
	struct serializer<::base::OwningView> {
		static constexpr Errc write(writer auto& ar, const ::base::OwningView& v) {
			return detail::writeByteBlob(ar, v.view());
		}

		static constexpr Errc read(reader auto& ar, ::base::OwningView& v) {
			::std::byte*  raw = nullptr;
			::std::size_t n   = 0;
			if (const auto c = detail::readByteBlob(ar, raw, n); c != Errc::Ok) return c;
			// Move assignment frees whatever the view held before, which is what a read
			// into an existing object has to do.
			v = ::base::OwningView(raw, n);
			return Errc::Ok;
		}
	};

	// ── SharedView ────────────────────────────────────────────────────────────

	template<>
	struct min_wire_size<::base::SharedView> {
		static constexpr ::std::size_t VALUE = sizeof(detail::wire_size_type);
	};

	template<>
	struct schema<::base::SharedView> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaByteBlob<Mode, Seen>(h);
		}
	};

	template<>
	struct serializer<::base::SharedView> {
		static constexpr Errc write(writer auto& ar, const ::base::SharedView& v) {
			return detail::writeByteBlob(ar, v.view());
		}

		static constexpr Errc read(reader auto& ar, ::base::SharedView& v) {
			::std::byte*  raw = nullptr;
			::std::size_t n   = 0;
			if (const auto c = detail::readByteBlob(ar, raw, n); c != Errc::Ok) return c;
			v = ::base::SharedView(raw, n);
			return Errc::Ok;
		}

		// SharedView has no default constructor, so dispatchMake cannot build one by
		// reading into a fresh object - without this it could not be a top-level
		// ser::read, an element of a container, or a field of a type that has to be built.
		static ::base::SharedView make(reader auto& ar) {
			::std::byte*  raw = nullptr;
			::std::size_t n   = 0;
			if (const auto c = detail::readByteBlob(ar, raw, n); c != Errc::Ok)
				throwError(c, ar.position());
			return { raw, n };
		}
	};

}  // namespace ser
