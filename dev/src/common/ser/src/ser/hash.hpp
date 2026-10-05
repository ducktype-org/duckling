#pragma once

/**
 * @file
 * @brief schemaHash<T, Ctx>()
 * @details One 64-bit number that says "this is the format I write". It goes into the envelope
 * (stream/header.hpp) and is checked before a single payload byte is interpreted, so a
 * stream written before a field was added comes back as Errc::SchemaMismatch instead of
 * plausible garbage. It takes Ctx because the pools registered in a context are part of
 * the format.
 *
 * It hashes the SERIALIZED FORM, one field at a time: sizeof and alignof are never mixed for a type
 * whose fields are walked, because the stream has neither padding nor alignment. Same rule
 * as bool, whose serialized size is 1 and never sizeof(bool).
 *
 * IT IS NOT A PORTABLE NUMBER, AND THAT IS THE POINT. schemaRoot() mixes nativeFlags(), so
 * the question it answers is "can THIS program read that stream" rather than "do two
 * platforms describe the same format" - and a pinned literal is pinned per platform and
 * per toolchain.
 *
 * What is visible, and how much:
 *
 *   scalar / enum / fixed array   the serialized kind and width, recursively
 *   a walked aggregate            the field count and every field's schema
 *   SER_DESCRIBE / _MAKE          the DESCRIBED field list - which is the format those
 *                                 macros emit, so a described aggregate and the same
 *                                 aggregate walked automatically hash IDENTICALLY
 *   std adapters                  a structural token per container (ser::Schema<T>)
 *   ser_serialize_as (+ _tag)         the serialized type it names, behind an optional token - the
 *                                 in-class form of the same thing, for a type that cannot
 *                                 reach into namespace ser
 *   a hand-written hook           "hook", sizeof, alignof
 *   ser::Config<T>::schema_id     that number, and nothing else
 *
 * A hooked type is the weak spot: sizeof and alignof are all there is, so two unrelated
 * hooked types of the same size and alignment share a hash. Three ways out, in the order
 * schemaOf consults them - ser::Config<T>::schema_id, a ser::Schema<T> specialization, or
 * an in-class `ser_serialize_as` alias (what every STRONG_TYPEDEF_INT uses).
 *
 * NO FIELD NAMES: the ladder does not know them, and byte identity with a reflection-based
 * implementation outranks a stronger hash. Names go into debugHash(), which never reaches
 * a stream. Two same-typed fields swapped is the one change no hash of this kind can see,
 * and SER_TEST_ROUNDTRIP in <ser/test.hpp> is what catches it.
 */

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>

#include <ser/archive/out.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/internal/access_detect.hpp>
#include <ser/internal/describe.hpp>
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

	/**
	 * @brief ser::Schema<T>
	 * @details The extension point for "I know what this type's format is, hash THAT". Same shape as
	 * ser::Serializer<T>: an empty primary, and a specialization that wins. Every std
	 * adapter has one, which is what keeps a container's hash structural - vector<int> and
	 * vector<float> differ - rather than a sizeof of somebody's std::vector.
	 *
	 *     template <class T, class Al>
	 *     struct ser::Schema<std::vector<T, Al>> {
	 *         template <class Mode, class Seen>
	 *         static consteval ::std::uint64_t mix(::std::uint64_t h) {
	 *             return ser::internal::schemaOf<T, Mode, Seen>(
	 *                        ser::internal::schemaText(h, "vector"));
	 *         }
	 *     };
	 *
	 * Mode and Seen are passed straight through and never inspected. No token is injected
	 * before the call, so a serializer that writes exactly a std::uint32_t can hash
	 * identically to the scalar it is. A type that cannot reach into namespace ser says the
	 * same thing in-class with `using ser_serialize_as = W;` - see ser::Access.
	 */
	template<class T>
	struct Schema {};

	/**
	 * @internal Not part of the public surface. `ser::Schema<T>` above and `schemaHash` /
	 * `debugHash` below are; everything in this namespace is how they are computed.
	 * @note It stays in this header rather than moving under `internal/` because the walk
	 * needs `ser::Schema<T>` complete, and every adapter that specializes `Schema<T>` also
	 * calls `schemaOf` / `schemaText` - splitting them would make an adapter include an
	 * `internal/` header to reach a public extension point.
	 */
	namespace internal {

		/**
		 * @brief FNV-1a, 64-bit
		 * @details Not a cryptographic hash and does not need to be: the question is only "is this
		 * the same format". Chosen because it is four lines, needs no tables, and is
		 * byte-for-byte reproducible in any constant evaluation on any compiler.
		 */
		inline constexpr ::std::uint64_t FNV_BASIS = 0xcb'f2'9c'e4'84'22'23'25ull;
		inline constexpr ::std::uint64_t FNV_PRIME = 0x00'00'01'00'00'00'01'b3ull;

		[[nodiscard]] constexpr ::std::uint64_t hashByte(
			::std::uint64_t h, unsigned char b
		) noexcept {
			return (h ^ static_cast<::std::uint64_t>(b)) * FNV_PRIME;
		}

		/**
		 * @brief Every token ends with a zero byte, so a sequence cannot be re-cut: "vector"
		 * then "int" is not "vectorint", and a schema is exactly a sequence.
		 */
		[[nodiscard]] constexpr ::std::uint64_t schemaText(
			::std::uint64_t h, const char* s
		) noexcept {
			for (; *s != '\0'; ++s) h = hashByte(h, static_cast<unsigned char>(*s));
			return hashByte(h, 0u);
		}

		/**
		 * @brief Eight bytes, least significant first, whatever the platform's byte order is -
		 * the hash must not depend on it. nativeFlags() already carries the order, and
		 * carries it once.
		 */
		[[nodiscard]] constexpr ::std::uint64_t schemaNumber(
			::std::uint64_t h, ::std::uint64_t v
		) noexcept {
			for (int i = 0; i < 8; ++i) {
				h = hashByte(h, static_cast<unsigned char>(v & 0xFFu));
				v >>= 8;
			}
			return h;
		}

	}  // namespace internal

	/**
	 * @brief nativeFlags()
	 * @details The platform facts a stream cannot survive a change in, in sixteen bits. The
	 * envelope carries them and reports Errc::PlatformMismatch before the schema is even looked at,
	 * because a stream from the other byte order is not a stream with a different schema -
	 * every scalar in it is reversed.
	 *
	 * Layout, frozen:  bits 0-1  byte order (1 little, 2 big, 3 neither)
	 *                  bits 2-5  sizeof(void*)
	 *                  bits 6-9  sizeof(ConfigGlobal::size_type)
	 *
	 * It is in the schema hash as well, and the two are not redundant: a stream written
	 * WITHOUT an envelope has only the hash. It lives here rather than in
	 * stream/header.hpp because schema_hash mixes it and this header cannot include that
	 * one.
	 */
	[[nodiscard]] consteval ::std::uint16_t nativeFlags() noexcept {
		constexpr unsigned ORDER = (::std::endian::native == ::std::endian::little) ? 1u
		                         : (::std::endian::native == ::std::endian::big)    ? 2u
		                                                                            : 3u;
		constexpr unsigned BITS  = ORDER | (static_cast<unsigned>(sizeof(void*)) << 2)
		                        | (static_cast<unsigned>(sizeof(ConfigGlobal::SizeType)) << 6);
		static_assert(
			BITS <= 0xFF'FFu,
			"ser: nativeFlags does not fit in 16 bits - a platform with a "
			"pointer or size_type wider than 15 bytes needs a wider field."
		);
		return static_cast<::std::uint16_t>(BITS);
	}

	/** @internal The schema walk itself - see the note on the first `internal` block. */
	namespace internal {

		/**
		 * @brief Whether names are mixed, and which context the format is for. One type
		 * parameter instead of two so that ser::Schema<T> specializations forward it
		 * blind and never have to change when a third knob shows up.
		 */
		template<class Ctx, bool Names>
		struct SchemaMode final {
			using Context               = Ctx;
			static constexpr bool NAMES = Names;
		};

		template<class T, class Mode, class Seen>
		[[nodiscard]] consteval ::std::uint64_t schemaOf(::std::uint64_t h);

		/**
		 * the leaves
		 * The token is the serialized KIND and the serialized WIDTH, never the C++ type's name. So
		 * int64_t hashes the same whether it spells itself `long` or `long long`, while
		 * long double does not hash as double - that really is a different number of bytes.
		 * char, wchar_t and the char*_t family share the "char" kind, because whether plain
		 * char is signed is a platform property that never reaches the stream.
		 */
		static_assert(
			builtin::SCALAR_SERIALIZED_SIZE<bool> == 1,
			"ser: bool is one byte in the stream and schema_hash must hash that 1, "
			"not sizeof(bool) - otherwise two platforms agreeing on the format "
			"disagree on the hash."
		);

		template<class T>
		[[nodiscard]] consteval ::std::uint64_t schemaScalar(::std::uint64_t h) {
			using U              = ::std::remove_cv_t<T>;
			constexpr auto WIDTH = static_cast<::std::uint64_t>(builtin::SCALAR_SERIALIZED_SIZE<U>);

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

		/** @brief the two overrides, and the two field lists */
		template<class T>
		inline constexpr bool HAS_SCHEMA_ID_V = requires {
			{ Config<::std::remove_cv_t<T>>::schema_id } -> ::std::convertible_to<::std::uint64_t>;
		};

		template<class T, class Mode, class Seen>
		inline constexpr bool HAS_SCHEMA_MIX_V = requires(::std::uint64_t h) {
			{
				Schema<::std::remove_cv_t<T>>::template mix<Mode, Seen>(h)
			} -> ::std::convertible_to<::std::uint64_t>;
		};

		/**
		 * @brief The in-class alias, and its optional name token - see the note in ser::Access.
		 * The token comes FIRST, so a named wrapper is a different format from the type it
		 * wraps while an unnamed one is the same format, byte for byte and hash for hash.
		 */
		template<class T, class Mode, class Seen>
		[[nodiscard]] consteval ::std::uint64_t schemaAs(::std::uint64_t h) {
			using U = ::std::remove_cv_t<T>;
			if constexpr (Access::HAS_SCHEMA_TAG_V<U>) h = schemaText(h, Access::schemaTag<U>());
			return schemaOf<::std::remove_cv_t<Access::SerializeAsT<U>>, Mode, Seen>(h);
		}

		/** @brief Diagnostics only: debugHash mixes the field name when SER_DESCRIBE left one. */
		template<class T, class Mode>
		[[nodiscard]] consteval ::std::uint64_t schemaFieldName(::std::uint64_t h, ::std::size_t i) {
			if constexpr (Mode::NAMES && Access::HAS_FIELD_NAMES_V<T>)
				return schemaText(h, Access::fieldName<T>(i));
			else
				return (void) i, h;
		}

		template<class T, class Mode, class Seen, class... Fs>
		[[nodiscard]] consteval ::std::uint64_t schemaFieldList(::std::uint64_t h, ::base::TypeList<Fs...>) {
			::std::size_t i = 0;
			((h = schemaOf<Fs, Mode, Seen>(h), h = schemaFieldName<T, Mode>(h, i), ++i), ...);
			return h;
		}

		/**
		 * @brief The same tokens for a walked aggregate and for a described one, because they are
		 * the same bytes: adding SER_DESCRIBE(a, b) to an aggregate whose fields are
		 * exactly a and b does not invalidate a single stream.
		 */
		template<class T, class Mode, class Seen, class... Fs>
		[[nodiscard]] consteval ::std::uint64_t schemaStruct(
			::std::uint64_t h, ::base::TypeList<Fs...> fields
		) {
			return schemaFieldList<T, Mode, Seen>(
				schemaNumber(schemaText(h, "struct"), sizeof...(Fs)), fields
			);
		}

		/**
		 * @brief Everything the library cannot see through. sizeof and alignof are the whole of
		 * it, which is weaker than a field list and stronger than nothing - see the note
		 * at the top of the file, and ser::Config<T>::schema_id for the way out.
		 */
		template<class T>
		[[nodiscard]] consteval ::std::uint64_t schemaOpaque(::std::uint64_t h, const char* kind) {
			using U = ::std::remove_cv_t<T>;
			return schemaNumber(schemaNumber(schemaText(h, kind), sizeof(U)), alignof(U));
		}

		/**
		 * @brief which level's hook is the format
		 * @details A trait-level hook outranks the in-class one, so a type carrying both
		 * SER_DESCRIBE and a ser::Serializer<T> specialization is written by the
		 * specialization, and its described field list is NOT the format.
		 */
		template<class T, class Ar>
		inline constexpr bool TRAIT_LEVEL_HOOK_V
			= LVL_WRITE_V<Access::TraitHooks, T, Ar> || LVL_READ_V<Access::TraitHooks, T, Ar>
		   || LVL_MAKE_V<Access::TraitHooks, T, Ar> || LVL_VISIT_WRITE_V<Access::TraitHooks, T, Ar>
		   || LVL_VISIT_READ_V<Access::TraitHooks, T, Ar>;

		/**
		 * @brief the recursion
		 * @details Seen is the PATH, not a visited set: a type met twice on two different branches
		 * is hashed twice, and a type met inside itself becomes a back-reference. That is
		 * what makes `struct Tree { int v; std::vector<Tree> kids; };` terminate, and the
		 * index is what keeps two different recursion shapes apart.
		 */
		template<class T, class Mode, class Seen>
		[[nodiscard]] consteval ::std::uint64_t schemaOf(::std::uint64_t h) {
			using U = ::std::remove_cv_t<T>;

			if constexpr (::base::LIST_CONTAINS_V<U, Seen>)
				return schemaNumber(schemaText(h, "recur"), ::base::LIST_INDEX_OF_V<U, Seen>);
			else {
				using Next = ::base::CatListsT<Seen, ::base::TypeList<U>>;

				// The hook question needs an archive and a hash cannot have one, so it is
				// asked with the canonical writer for this context - the same shape
				// internal::writer_for builds.
				using Ar = Out<::std::span<::std::byte>, typename Mode::Context>;

				if constexpr (HAS_SCHEMA_ID_V<U>)
					return schemaNumber(
						schemaText(h, "id"), static_cast<::std::uint64_t>(Config<U>::schema_id)
					);
				else if constexpr (HAS_SCHEMA_MIX_V<U, Mode, Next>)
					return Schema<U>::template mix<Mode, Next>(h);
				else if constexpr (Access::HAS_SERIALIZE_AS_V<U>)
					return schemaAs<U, Mode, Next>(h);
				else if constexpr (HAS_CUSTOM_SERIALIZER_V<U, Ar>)
					if constexpr (Access::HAS_DESCRIBED_V<U> && !TRAIT_LEVEL_HOOK_V<U, Ar>)
						return schemaStruct<U, Mode, Next>(h, DescribedTypesT<U>{});
					else
						// @TODO: #3719 make this a compile error that names the three ways out
						return schemaOpaque<U>(h, "hook");
				else if constexpr (builtin::ScalarLike<U>)
					return schemaScalar<U>(h);
				else if constexpr (builtin::EnumLike<U>)
					return schemaScalar<::std::underlying_type_t<U>>(schemaText(h, "enum"));
				else if constexpr (CAN_ENUMERATE_MEMBERS_V<U>)
					return schemaStruct<U, Mode, Next>(h, FieldTypesT<U>{});
				else
					// Nothing serializes this type either - dispatch refuses it with a
					// list of fixes. Answering rather than failing keeps schema_hash
					// usable as a question.
					//
					// @TODO: #3719 make this a compile error as well
					return schemaOpaque<U>(h, "opaque");
			}
		}

		/**
		 * @brief the preamble
		 * @details Mixed once, at the root, so the recursion stays a pure description of the type.
		 * The library version is in deliberately: while the format is pre-1.0 a version
		 * bump is a format break.
		 */
		template<class T, class Mode>
		[[nodiscard]] consteval ::std::uint64_t schemaRoot() {
			::std::uint64_t h = FNV_BASIS;
			h                 = schemaText(h, Mode::NAMES ? "ser.debug.1" : "ser.schema.1");
			h                 = schemaNumber(h, static_cast<::std::uint64_t>(VERSION));
			h                 = schemaNumber(h, sizeof(ConfigGlobal::SizeType));
			h                 = schemaNumber(h, nativeFlags());
			// The pool count is the whole of it for now, and it is what makes
			// Context<str_pool> and Context<> different formats for free - no bytes in
			// the stream, no runtime check.
			h = schemaText(h, "ctx");
			h = schemaNumber(h, static_cast<::std::uint64_t>(Mode::Context::POOL_COUNT));
			return schemaOf<T, Mode, ::base::TypeList<>>(h);
		}

	}  // namespace internal

	/**
	 * @brief The number that goes into the envelope.
	 *     static_assert(ser::schemaHash<Config>() == 0x...);
	 */
	template<class T, class Ctx = NoContext>
	[[nodiscard]] consteval ::std::uint64_t schemaHash() {
		return internal::schemaRoot<T, internal::SchemaMode<Ctx, false>>();
	}

	/**
	 * @brief The same walk with field names mixed in. DIAGNOSTICS ONLY - it never goes into a
	 * stream and nothing validates against it. Use it to tell "the struct changed" from
	 * "only a name changed": schemaHash equal and debugHash different means a rename.
	 */
	template<class T, class Ctx = NoContext>
	[[nodiscard]] consteval ::std::uint64_t debugHash() {
		return internal::schemaRoot<T, internal::SchemaMode<Ctx, true>>();
	}

}  // namespace ser
