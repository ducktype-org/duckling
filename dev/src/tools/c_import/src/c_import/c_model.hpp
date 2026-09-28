/**
 * @file c_model.hpp
 * @brief Plain description of the C declarations read from a translation unit.
 *
 * Only the reader touches libclang; everything downstream works on this model, so it can be
 * built by hand in tests.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace c_import {

	struct CType;
	using CTypeRef = std::shared_ptr<const CType>;

	enum class ScalarKind : std::uint8_t {
		Bool,
		/// Plain `char`, whose signedness is a property of the target.
		Char,
		SignedInt,
		UnsignedInt,
		Float,
	};

	struct CScalar final {
		ScalarKind    kind;
		std::uint32_t bits;

		bool operator==(const CScalar&) const = default;
	};

	/// `void`, only meaningful as a return type or a pointee.
	struct CVoid final {};

	struct CPointer final {
		CTypeRef pointee;
	};

	/// A fixed-size array; `count` is 0 for a flexible array member.
	struct CArray final {
		CTypeRef      element;
		std::uint64_t count;
	};

	/// A reference to a record by its index in `CModel::records`.
	struct CRecordRef final {
		std::size_t index;
	};

	struct CFunctionType final {};

	/// A type with no Duckling mapping, such as `long double` or `__int128`.
	struct CUnsupported final {
		std::string reason;
	};

	struct CType final {
		std::variant<CScalar, CVoid, CPointer, CArray, CRecordRef, CFunctionType, CUnsupported> kind;
	};

	/// Where a declaration comes from, used to keep only the requested headers' declarations.
	struct CLocation final {
		std::string file;
		bool        in_requested_headers = false;
	};

	struct CField final {
		/// Empty for an anonymous record member.
		std::string                  name;
		CTypeRef                     type;
		std::uint64_t                offset_bits = 0;
		std::optional<std::uint32_t> bit_width;
		/// Size and alignment of the field's type, in bytes.
		std::uint64_t size  = 0;
		std::uint64_t align = 0;
	};

	struct CRecord final {
		std::string         name;
		bool                is_union  = false;
		bool                complete  = false;
		bool                anonymous = false;
		std::uint64_t       size      = 0;
		std::uint64_t       align     = 0;
		std::vector<CField> fields;
		CLocation           location;
	};

	struct CParam final {
		std::string name;
		CTypeRef    type;
	};

	struct CFunction final {
		std::string         name;
		CTypeRef            return_type;
		std::vector<CParam> params;
		bool                variadic = false;
		/// `static` or `static inline`: no symbol to link against.
		bool      internal_linkage = false;
		CLocation location;
	};

	struct CEnumerator final {
		std::string                               name;
		std::variant<std::int64_t, std::uint64_t> value;
	};

	struct CEnum final {
		/// Empty for an anonymous enum.
		std::string              name;
		CScalar                  underlying;
		std::vector<CEnumerator> enumerators;
		CLocation                location;
	};

	/// An object-like macro, or a `static const` of the requested headers, that evaluates to a
	/// number.
	struct CConstant final {
		std::string                                       name;
		CScalar                                           type;
		std::variant<std::int64_t, std::uint64_t, double> value;
		CLocation                                         location;
	};

	/// A declaration that could not be read at all, kept so it is reported.
	struct CUnreadable final {
		std::string name;
		std::string reason;
		CLocation   location;
	};

	struct CModel final {
		std::vector<CRecord>     records;
		std::vector<CFunction>   functions;
		std::vector<CEnum>       enums;
		std::vector<CConstant>   constants;
		std::vector<CUnreadable> unreadable;
		bool                     char_is_signed = true;
	};

	inline CTypeRef makeType(auto kind) {
		return std::make_shared<const CType>(CType{ std::move(kind) });
	}

}
