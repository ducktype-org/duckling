#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace c_import {

	/**
	 * @brief A Duckling scalar a C scalar can be mapped onto.
	 *
	 * @note C `char` maps to Int8/UInt8 rather than Duckling `char`: Duckling `char` lowers to
	 *       an unsigned 8-bit integer, so a signed C `char` parameter would be zero-extended
	 *       instead of sign-extended.
	 */
	enum class Scalar : std::uint8_t {
		Int8,
		Int16,
		Int32,
		Int64,
		UInt8,
		UInt16,
		UInt32,
		UInt64,
		Float32,
		Float64,
		Bool,
		/** Only reachable as the pointee of `cptr char`. */
		Char,
	};

	struct CType;
	using CTypeRef = std::shared_ptr<const CType>;

	struct ScalarType final {
		Scalar scalar;
	};

	/** An absent pointee renders as `cptr u8`: C `void*`, or a pointer to something skipped. */
	struct PointerType final {
		CTypeRef pointee;
	};

	struct RecordType final {
		std::string emitted_name;
	};

	struct ArrayType final {
		CTypeRef      element;
		std::uint64_t count;
	};

	/** A type with no admissible Duckling spelling. Carries the reason for the skip comment. */
	struct UnsupportedType final {
		std::string reason;
	};

	struct CType final {
		std::variant<ScalarType, PointerType, RecordType, ArrayType, UnsupportedType> kind;
	};

	struct CField final {
		std::string name;
		CType       type;
	};

	struct CRecord final {
		std::string         emitted_name;
		std::vector<CField> fields;
		/** Stem of the header the declaration came from. Only used when splitting. */
		std::string origin = {};
	};

	struct CParam final {
		std::string name;
		CType       type;
	};

	struct CFunction final {
		std::string         name;
		std::vector<CParam> params;
		/** Absent means the function returns C `void`. */
		std::shared_ptr<const CType> return_type;
		/** Stem of the header the declaration came from. Only used when splitting. */
		std::string origin = {};
	};

	/** An enumerator or an object-like macro, emitted outside the `extern("C")` block. */
	struct CConst final {
		std::string name;
		/** Rendered Duckling type, e.g. `u32`. */
		std::string type;
		/** Rendered literal including its suffix, e.g. `7u32`. */
		std::string literal;
		/** Stem of the header the declaration came from. Only used when splitting. */
		std::string origin = {};
	};

	struct CAlias final {
		std::string name;
		std::string target;
		/** Stem of the header the declaration came from. Only used when splitting. */
		std::string origin = {};
	};

	struct CSkipped final {
		std::string kind;
		std::string name;
		std::string reason;
		/** Stem of the header the declaration came from. Only used when splitting. */
		std::string origin = {};
	};

	using CDecl = std::variant<CRecord, CFunction, CConst, CAlias, CSkipped>;

	struct TranslationUnitModel final {
		std::vector<CDecl> decls;
	};

	CType makeScalar(Scalar scalar);
	CType makePointer(CType pointee);
	/** `cptr u8`: C `void*`, or a pointer to a type that was skipped. */
	CType makeOpaquePointer();
	CType makeRecord(std::string emitted_name);
	CType makeArray(CType element, std::uint64_t count);
	CType makeUnsupported(std::string reason);

}
