#include "type_mapper.hpp"

#include <variant>

namespace c_import {

	std::string renderScalar(Scalar scalar) {
		switch (scalar) {
		case Scalar::Int8:
			return "i8";
		case Scalar::Int16:
			return "i16";
		case Scalar::Int32:
			return "i32";
		case Scalar::Int64:
			return "i64";
		case Scalar::UInt8:
			return "u8";
		case Scalar::UInt16:
			return "u16";
		case Scalar::UInt32:
			return "u32";
		case Scalar::UInt64:
			return "u64";
		case Scalar::Float32:
			return "f32";
		case Scalar::Float64:
			return "f64";
		case Scalar::Bool:
			return "bool";
		case Scalar::Char:
			return "char";
		}
		return {};
	}

	std::string renderType(const CType& type) {
		return std::visit(
			[](const auto& kind) -> std::string {
				using T = std::decay_t<decltype(kind)>;
				if constexpr (std::is_same_v<T, ScalarType>) {
					return renderScalar(kind.scalar);
				} else if constexpr (std::is_same_v<T, PointerType>) {
					if (kind.pointee == nullptr || !isSupported(*kind.pointee)) return "cptr u8";
					return "cptr " + renderType(*kind.pointee);
				} else if constexpr (std::is_same_v<T, RecordType>) {
					return kind.emitted_name;
				} else if constexpr (std::is_same_v<T, ArrayType>) {
					return renderType(*kind.element) + "[" + std::to_string(kind.count) + "]";
				} else {
					return {};
				}
			},
			type.kind
		);
	}

	bool isSupported(const CType& type) { return unsupportedReason(type).empty(); }

	std::string unsupportedReason(const CType& type) {
		return std::visit(
			[](const auto& kind) -> std::string {
				using T = std::decay_t<decltype(kind)>;
				if constexpr (std::is_same_v<T, UnsupportedType>) {
					return kind.reason;
				} else if constexpr (std::is_same_v<T, ArrayType>) {
					// A zero-length array has no admissible spelling, so a record carrying a
				    // flexible array member is skipped along with it.
					if (kind.count == 0) return "zero-length array";
					return unsupportedReason(*kind.element);
				} else {
					// Pointers stay supported even when the pointee is not: they degrade to
				    // `cptr u8`.
					return {};
				}
			},
			type.kind
		);
	}

}
