#pragma once

// ── schemaHash<T, Ctx>() ─────────────────────────────────────────────────────
// One 64-bit number that says "this is the format I write". It goes into the envelope
// (stream/header.hpp) and is checked before a single payload byte is interpreted, so a
// stream from before a field was added comes back as Errc::SchemaMismatch instead of
// plausible garbage. Its real value is during development rather than in production:
// change a struct, rebuild, and the cache written twenty minutes ago is stale - without
// the hash you read nonsense, with it you get an error code and rebuild the cache.
//
// It takes Ctx from the first version, exactly like is_flat_v takes Ar, and for the same
// reason: in M2 the set of pools registered in the context is part of the format, so
// "which schema" and "which context" cannot be allowed to disagree. Mixing the context
// is also what makes a Ctx mismatch detectable at all - see CLAUDE.md on the read-side
// completeness check. In M1 there is one context and it mixes as POOL_COUNT 0.
//
// ── it hashes the WIRE, and falls back to the OBJECT only where it must ───────
// This is the one design decision in the file and it is deliberate. sizeof and alignof
// are NOT mixed for a type whose fields are walked, because the wire has neither padding
// nor alignment: `struct { std::uint8_t a; std::uint32_t b; }` is seven bytes on every
// platform, and hashing sizeof would make two platforms that agree on the format
// disagree on the hash. That is the rule CLAUDE.md states for bool - the wire size is 1,
// never sizeof(bool) - applied to composites.
//
// A type whose hook the library cannot look inside is the exception, and there sizeof and
// alignof are all there is. The cost is real and worth naming: sizeof of a type holding a
// std::string differs between standard libraries, so such a type's hash is stable per
// toolchain rather than per format. ser::config<T>::schema_id is the fix, and the only
// way to make a hooked type's hash portable.
//
// What is visible, and how much:
//
//   scalar / enum / fixed array   the wire kind and width, recursively
//   aggregate walked by step 8    the field count and every field's schema
//   SER_DESCRIBE / _MAKE          the DESCRIBED field list - which is the format those
//                                 macros emit, so a described aggregate and the same
//                                 aggregate walked automatically hash IDENTICALLY
//   std adapters                  a structural token per container (ser::schema<T>)
//   a hand-written hook           "hook", sizeof, alignof
//   ser::config<T>::schema_id     that number, and nothing else
//
// NO FIELD NAMES. The structured-bindings ladder does not know them, so hashing them
// would make the C++23 and C++26 backends disagree - and byte identity between the two
// outranks a stronger hash. Names go into debugHash(), which is diagnostics only and
// never reaches a stream.
//
// Two same-typed fields swapped is the one change no hash of this kind can see. That is
// what SER_TEST_ROUNDTRIP in <ser/test.hpp> is for.

#include <ser/access.hpp>
#include <ser/archive/out.hpp>
#include <ser/builtin/array.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/detail/describe.hpp>
#include <ser/detail/meta.hpp>
#include <ser/pool/context.hpp>
#include <ser/type_config.hpp>

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <type_traits>

namespace ser {

	// ── ser::schema<T> ────────────────────────────────────────────────────────
	// The extension point for "I know what this type's format is, hash THAT". Same shape
	// and same job as ser::serializer<T>: an empty primary, and a specialization that
	// wins. Every std adapter has one, which is what keeps a container's hash structural
	// (vector<int> and vector<float> differ) instead of a sizeof of somebody's
	// std::vector implementation.
	//
	//     template <class T, class Al>
	//     struct ser::schema<std::vector<T, Al>> {
	//         template <class Mode, class Seen>
	//         static consteval ::std::uint64_t mix(::std::uint64_t h) {
	//             return ser::detail::schemaOf<T, Mode, Seen>(
	//                        ser::detail::schemaText(h, "vector"));
	//         }
	//     };
	//
	// Mode and Seen are passed straight through and never inspected: Mode carries the
	// context and whether names are being mixed, Seen is the recursion path that makes a
	// self-referential type terminate. Bundling them into Mode is what lets a later knob
	// arrive without touching a single adapter.
	//
	// No token is injected before the call, on purpose: a serializer that writes exactly
	// a std::uint32_t can say so with `return detail::schemaOf<std::uint32_t, Mode,
	// Seen>(h);` and hash identically to the scalar it is.
	//
	// Same include rule as min_wire_size, and for the same reason: a specialization has to
	// be visible before the first schema_hash of a type that needs it. <ser/std/all.hpp>
	// before the first ser::write of such a type already satisfies it - a type whose
	// adapter is missing cannot be serialized at all, so there is no way to end up with a
	// stream whose hash was computed without one.
	template<class T>
	struct schema {};

	namespace detail {

		// ── FNV-1a, 64-bit ────────────────────────────────────────────────────
		// Not a cryptographic hash and does not need to be: the question is "is this the
		// same format", and the answer only has to be wrong less often than the machine
		// is. Chosen because it is four lines, needs no tables, and is byte-for-byte
		// reproducible in any constant evaluation on any compiler.
		inline constexpr ::std::uint64_t FNV_BASIS = 0xcb'f2'9c'e4'84'22'23'25ull;
		inline constexpr ::std::uint64_t FNV_PRIME = 0x00'00'01'00'00'00'01'b3ull;

		[[nodiscard]] constexpr ::std::uint64_t hashByte(
			::std::uint64_t h, unsigned char b
		) noexcept {
			return (h ^ static_cast<::std::uint64_t>(b)) * FNV_PRIME;
		}

		// Every token ends with a zero byte, so a sequence cannot be re-cut: "vector"
		// then "int" is not "vectorint", and a schema is exactly a sequence.
		[[nodiscard]] constexpr ::std::uint64_t schemaText(
			::std::uint64_t h, const char* s
		) noexcept {
			for (; *s != '\0'; ++s) h = hashByte(h, static_cast<unsigned char>(*s));
			return hashByte(h, 0u);
		}

		// Eight bytes, least significant first, whatever the platform's byte order is -
		// the hash must not depend on it. nativeFlags() already carries the order, and
		// carries it once.
		[[nodiscard]] constexpr ::std::uint64_t schemaNumber(
			::std::uint64_t h, ::std::uint64_t v
		) noexcept {
			for (int i = 0; i < 8; ++i) {
				h = hashByte(h, static_cast<unsigned char>(v & 0xFFu));
				v >>= 8;
			}
			return h;
		}

	}  // namespace detail

	// ── nativeFlags() ────────────────────────────────────────────────────────
	// The platform facts a stream cannot survive a change in, in sixteen bits. The
	// envelope carries them and refuses a mismatch with Errc::PlatformMismatch before
	// the schema is even looked at, because a stream from the other byte order is not a
	// stream with a different schema - every scalar in it is reversed.
	//
	// Layout, frozen:  bits 0-1  byte order (1 little, 2 big, 3 neither)
	//                  bits 2-5  sizeof(void*)
	//                  bits 6-9  sizeof(config_global::size_type)
	//
	// It lives here rather than in stream/header.hpp because schema_hash mixes it and
	// this header cannot include that one. The pointer size is in for a reason that only
	// pays off later: nothing writes a pointer, but pool offsets and zero-copy alignment
	// are sized by it, so a 32-bit reader must not accept a 64-bit writer's stream.
	[[nodiscard]] consteval ::std::uint16_t nativeFlags() noexcept {
		constexpr unsigned ORDER = (::std::endian::native == ::std::endian::little) ? 1u
		                         : (::std::endian::native == ::std::endian::big)    ? 2u
		                                                                            : 3u;
		constexpr unsigned BITS  = ORDER | (static_cast<unsigned>(sizeof(void*)) << 2)
		                        | (static_cast<unsigned>(sizeof(config_global::size_type)) << 6);
		static_assert(
			BITS <= 0xFF'FFu,
			"ser: nativeFlags does not fit in 16 bits - a platform with a "
			"pointer or size_type wider than 15 bytes needs a wider field."
		);
		return static_cast<::std::uint16_t>(BITS);
	}

	namespace detail {

		// Whether names are mixed, and which context the format is for. One type
		// parameter instead of two so that ser::schema<T> specializations forward it
		// blind and never have to change when a third knob shows up.
		template<class Ctx, bool Names>
		struct schema_mode {
			using context               = Ctx;
			static constexpr bool NAMES = Names;
		};

		template<class T, class Mode, class Seen>
		[[nodiscard]] consteval ::std::uint64_t schemaOf(::std::uint64_t h);

		// ── the leaves ────────────────────────────────────────────────────────
		// The token is the wire KIND and the wire WIDTH, never the C++ type's name. Two
		// consequences, both wanted:
		//   - int64_t hashes the same whether it spells itself `long` (Linux) or
		//     `long long` (Windows), so one struct hashes the same on both;
		//   - long double does NOT hash the same, because it really is a different number
		//     of bytes, and that is a format difference rather than a spelling.
		// char, wchar_t and the char*_t family share the "char" kind: whether plain char
		// is signed is a platform property that never reaches the wire, so hashing
		// signedness there would break agreement between platforms that agree.
		static_assert(
			builtin::SCALAR_WIRE_SIZE<bool> == 1,
			"ser: bool is one byte on the wire and schema_hash must hash that 1, "
			"not sizeof(bool) - otherwise two platforms agreeing on the format "
			"disagree on the hash. See CLAUDE.md."
		);

		template<class T>
		[[nodiscard]] consteval ::std::uint64_t schemaScalar(::std::uint64_t h) {
			using U              = ::std::remove_cv_t<T>;
			constexpr auto WIDTH = static_cast<::std::uint64_t>(builtin::SCALAR_WIRE_SIZE<U>);

			if constexpr (::std::is_same_v<U, bool>)
				return schemaNumber(schemaText(h, "bool"), WIDTH);
			else if constexpr (::std::is_same_v<U, ::std::byte>)
				return schemaNumber(schemaText(h, "byte"), WIDTH);
			else if constexpr (::std::is_same_v<U, char> || ::std::is_same_v<U, wchar_t>
			                   || ::std::is_same_v<U, char8_t> || ::std::is_same_v<U, char16_t>
			                   || ::std::is_same_v<U, char32_t>)
				return schemaNumber(schemaText(h, "char"), WIDTH);
			else if constexpr (::std::is_floating_point_v<U>)
				return schemaNumber(schemaText(h, "float"), WIDTH);
			else if constexpr (::std::is_signed_v<U>)
				return schemaNumber(schemaText(h, "int"), WIDTH);
			else
				return schemaNumber(schemaText(h, "uint"), WIDTH);
		}

		// ── the two overrides, and the two field lists ────────────────────────
		template<class T>
		inline constexpr bool HAS_SCHEMA_ID_V = requires {
			{ config<::std::remove_cv_t<T>>::schema_id } -> ::std::convertible_to<::std::uint64_t>;
		};

		template<class T, class Mode, class Seen>
		inline constexpr bool HAS_SCHEMA_MIX_V = requires(::std::uint64_t h) {
			{
				schema<::std::remove_cv_t<T>>::template mix<Mode, Seen>(h)
			} -> ::std::convertible_to<::std::uint64_t>;
		};

		// ser_described returns std::tie of the described fields, so its element types are
		// references - stripped here for the same reason field_types_t strips them: a
		// `const int` field goes on the wire as an int and has to hash as one.
		template<class T>
		struct tuple_field_list;

		template<class... Es>
		struct tuple_field_list<::std::tuple<Es...>> {
			using type = type_list<::std::remove_cvref_t<Es>...>;
		};

		template<class T>
		using described_types_t = typename tuple_field_list<
			decltype(access::described(::std::declval<const ::std::remove_cv_t<T>&>()))>::type;

		// Diagnostics only: debugHash mixes the field name when SER_DESCRIBE left one.
		template<class T, class Mode>
		[[nodiscard]] consteval ::std::uint64_t schemaFieldName(::std::uint64_t h, ::std::size_t i) {
			if constexpr (Mode::NAMES && access::HAS_FIELD_NAMES_V<T>)
				return schemaText(h, access::fieldName<T>(i));
			else
				return (void) i, h;
		}

		template<class T, class Mode, class Seen, class... Fs>
		[[nodiscard]] consteval ::std::uint64_t schemaFieldList(::std::uint64_t h, type_list<Fs...>) {
			::std::size_t i = 0;
			((h = schemaOf<Fs, Mode, Seen>(h), h = schemaFieldName<T, Mode>(h, i), ++i), ...);
			return h;
		}

		// The same tokens for a walked aggregate and for a described one, because they are
		// the same bytes: adding SER_DESCRIBE(a, b) to an aggregate whose fields are
		// exactly a and b does not invalidate a single stream.
		template<class T, class Mode, class Seen, class... Fs>
		[[nodiscard]] consteval ::std::uint64_t schemaStruct(
			::std::uint64_t h, type_list<Fs...> fields
		) {
			return schemaFieldList<T, Mode, Seen>(
				schemaNumber(schemaText(h, "struct"), sizeof...(Fs)), fields
			);
		}

		// Everything the library cannot see through. sizeof and alignof are the whole of
		// it, which is weaker than a field list and stronger than nothing - see the note
		// at the top of the file, and ser::config<T>::schema_id for the way out.
		template<class T>
		[[nodiscard]] consteval ::std::uint64_t schemaOpaque(::std::uint64_t h, const char* kind) {
			using U = ::std::remove_cv_t<T>;
			return schemaNumber(schemaNumber(schemaText(h, kind), sizeof(U)), alignof(U));
		}

		// ── which level's hook is the format ──────────────────────────────────
		// A trait-level hook outranks the in-class one, so a type carrying both
		// SER_DESCRIBE and a ser::serializer<T> specialization is written by the
		// specialization, and its described field list is NOT the format.
		template<class T, class Ar>
		inline constexpr bool TRAIT_LEVEL_HOOK_V
			= TRAIT_WRITE_V<T, Ar> || TRAIT_READ_V<T, Ar> || TRAIT_MAKE_V<T, Ar>
		   || TRAIT_VISIT_WRITE_V<T, Ar> || TRAIT_VISIT_READ_V<T, Ar>;

		// ── the recursion ─────────────────────────────────────────────────────
		// Seen is the PATH, not a visited set: a type met twice on two different branches
		// is hashed twice, and a type met inside itself becomes a back-reference. That is
		// what makes `struct Tree { int v; std::vector<Tree> kids; };` terminate, and the
		// index is what keeps two different recursion shapes apart.
		template<class T, class Mode, class Seen>
		[[nodiscard]] consteval ::std::uint64_t schemaOf(::std::uint64_t h) {
			using U = ::std::remove_cv_t<T>;

			if constexpr (CONTAINS_V<U, Seen>)
				return schemaNumber(schemaText(h, "recur"), INDEX_OF_V<U, Seen>);
			else {
				using Next = cat_lists_t<Seen, type_list<U>>;

				// The hook question needs an archive and a hash cannot have one, so it is
				// asked with the canonical writer for this context - the same shape
				// detail::writer_for builds. A hook visible only to some other concrete
				// archive is what nongeneric_*_hook_v reports; it is not a format this
				// can describe.
				using Ar = out<::std::span<::std::byte>, typename Mode::context>;

				if constexpr (HAS_SCHEMA_ID_V<U>)
					return schemaNumber(
						schemaText(h, "id"), static_cast<::std::uint64_t>(config<U>::schema_id)
					);
				else if constexpr (HAS_SCHEMA_MIX_V<U, Mode, Next>)
					return schema<U>::template mix<Mode, Next>(h);
				else if constexpr (HAS_CUSTOM_SERIALIZER_V<U, Ar>)
					if constexpr (access::HAS_DESCRIBED_V<U> && !TRAIT_LEVEL_HOOK_V<U, Ar>)
						return schemaStruct<U, Mode, Next>(h, described_types_t<U>{});
					else
						return schemaOpaque<U>(h, "hook");
				else if constexpr (builtin::scalar_like<U>)
					return schemaScalar<U>(h);
				else if constexpr (builtin::enum_like<U>)
					return schemaScalar<::std::underlying_type_t<U>>(schemaText(h, "enum"));
				else if constexpr (builtin::array_like<U>)
					return schemaOf<builtin::array_element_t<U>, Mode, Next>(
						schemaNumber(schemaText(h, "array"), builtin::ARRAY_LENGTH_V<U>)
					);
				else if constexpr (CAN_ENUMERATE_MEMBERS_V<U>)
					return schemaStruct<U, Mode, Next>(h, field_types_t<U>{});
				else
					// Nothing serializes this type either - dispatch refuses it with a
					// list of fixes. Answering rather than failing keeps schema_hash
					// usable as a question.
					return schemaOpaque<U>(h, "opaque");
			}
		}

		// ── the preamble ──────────────────────────────────────────────────────
		// Mixed once, at the root, so the recursion stays a pure description of the type.
		// The library version is in deliberately: while the format is pre-1.0 a version
		// bump is a format break, and pretending otherwise would hand somebody a stream
		// that validates and misreads.
		template<class T, class Mode>
		[[nodiscard]] consteval ::std::uint64_t schemaRoot() {
			::std::uint64_t h = FNV_BASIS;
			h                 = schemaText(h, Mode::NAMES ? "ser.debug.1" : "ser.schema.1");
			h                 = schemaNumber(h, static_cast<::std::uint64_t>(SER_VERSION));
			h                 = schemaNumber(h, sizeof(config_global::size_type));
			h                 = schemaNumber(h, nativeFlags());
			// M2 mixes each registered pool TYPE here. Until then the count is the whole
			// of it, and it is what makes context<str_pool> and context<> different
			// formats for free - no bytes in the stream, no runtime check.
			h = schemaText(h, "ctx");
			h = schemaNumber(h, static_cast<::std::uint64_t>(Mode::context::POOL_COUNT));
			return schemaOf<T, Mode, type_list<>>(h);
		}

	}  // namespace detail

	// The number that goes into the envelope. Pin it when the format matters:
	//     static_assert(ser::schemaHash<Config>() == 0x...);
	template<class T, class Ctx = no_context>
	[[nodiscard]] consteval ::std::uint64_t schemaHash() {
		return detail::schemaRoot<T, detail::schema_mode<Ctx, false>>();
	}

	// The same walk with field names mixed in. DIAGNOSTICS ONLY - it never goes into a
	// stream and nothing validates against it. Two builds of one program agree on it; the
	// C++23 and C++26 backends need not, which is precisely why the wire hash cannot have
	// names. Use it to tell "the struct changed" from "only a name changed": schema_hash
	// equal and debugHash different means a rename.
	template<class T, class Ctx = no_context>
	[[nodiscard]] consteval ::std::uint64_t debugHash() {
		return detail::schemaRoot<T, detail::schema_mode<Ctx, true>>();
	}

}  // namespace ser
