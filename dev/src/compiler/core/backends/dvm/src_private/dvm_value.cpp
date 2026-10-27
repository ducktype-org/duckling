// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "dvm_value.hpp"

#include "common.hpp"

#include <vm/bytecode/opcode_args.hpp>
#include <vm/core/builtin_functions.hpp>
#include <vm/utils/interpret.hpp>

using namespace compiler::backend_vm::internal;

namespace {
	vm::code::TypeExpr named(std::string_view name) {
		return vm::code::TypeExpr::type(base::StrID(name));
	}
}

#define DVM_NAMED_TYPE(func, name)                          \
	vm::code::TypeExpr dvm_type::func() {                   \
		static const vm::code::TypeExpr expr = named(name); \
		return expr;                                        \
	}

DVM_NAMED_TYPE(unit, "unit")
DVM_NAMED_TYPE(i8, "i8")
DVM_NAMED_TYPE(i16, "i16")
DVM_NAMED_TYPE(i32, "i32")
DVM_NAMED_TYPE(i64, "i64")
DVM_NAMED_TYPE(f32, "f32")
DVM_NAMED_TYPE(f64, "f64")
DVM_NAMED_TYPE(opaquePtr, "opaque_ptr")

#undef DVM_NAMED_TYPE

DVMPlace::DVMPlace(
	base::StrID                                name,
	vm::code::TypeExpr                         expr,
	base::Optional<CRef<vm::code::TypeOfData>> declaration,
	AccessKind                                 access_kind,
	SpecialKind                                special_kind
):
	  DVMPlace(name, expr, OperandKind::Unsupported, access_kind, special_kind) {
	// A pointer's operand kind follows from how it is written, with no declaration to consult.
	if (expr.isKind<vm::code::expr::Pointer>()) {
		operand_kind = OperandKind::Pointer;
		return;
	}
	if (expr.isKind<vm::code::expr::CPointer>()) {
		operand_kind = OperandKind::CPointer;
		return;
	}

	if (declaration.has_value()) {
		const vm::code::TypeOfData& type = **declaration;
		variant_match(type) {
			variant_case(vm::code::PrimitiveType, primitive) {
				if (primitive.size == Bytes{ 8 }) operand_kind = OperandKind::Place64;
				if (primitive.size == Bytes{ 4 }) operand_kind = OperandKind::Place32;
				if (primitive.size == Bytes{ 2 }) operand_kind = OperandKind::Place16;
				if (primitive.size == Bytes{ 1 }) operand_kind = OperandKind::Place8;
			}
			variant_case_novalue(vm::code::FixedSizeTableType) {
				operand_kind = OperandKind::FixedSizeTable;
			}
			variant_case_novalue(vm::code::DataType) { operand_kind = OperandKind::Structure; }
			variant_case_novalue(vm::code::VariantType) { operand_kind = OperandKind::Variant; }
			variant_case_novalue(vm::code::OpaqueType) { operand_kind = OperandKind::Opaque; }
			variant_default {}
		}
	}
}

[[nodiscard]] vm::opargs::OpCodeArg DVMPlace::asArgument() const {
	switch (operand_kind) {
	case OperandKind::Place8:
		return vm::opargs::Place8{ name };
	case OperandKind::Place16:
		return vm::opargs::Place16{ name };
	case OperandKind::Place32:
		return vm::opargs::Place32{ name };
	case OperandKind::Place64:
		return vm::opargs::Place64{ name };
	case OperandKind::FixedSizeTable:
		return vm::opargs::PlaceFSTable(name);
	case OperandKind::Structure:
		return vm::opargs::PlaceStructure(name);
	case OperandKind::Variant:
		return vm::opargs::PlaceVnt(name);
	case OperandKind::Opaque:
		return vm::opargs::PlaceOpq(name);
	case OperandKind::Pointer:
		return vm::opargs::PlacePtr(name);
	case OperandKind::CPointer:
		return vm::opargs::PlaceCPtr(name);
	case OperandKind::Unsupported:
		CORE_PANIC("DVMLocal type not supported for argument: ", expr.canonicalName());
	}
	CORE_UNREACHABLE();
}

[[nodiscard]] vm::opargs::OpCodeArg DVMPlace::asAnyArgument() const {
	return vm::opargs::PlaceAny{ name };
}

DVMPlace::operator vm::opargs::OpCodeArg() const { return asArgument(); }

[[nodiscard]] vm::opargs::OpCodeArg DVMValue::asArgument() const {
	return VISIT(stored_value, value, return value.asArgument());
}

[[nodiscard]] vm::opargs::OpCodeArg DVMValue::asAnyArgument() const {
	variant_match(stored_value) {
		variant_case(DVMPlace, place) return place.asAnyArgument();
		variant_default CORE_PANIC("Attempted to convert a non-anyable value to any argument");
	}
}

[[nodiscard]] vm::opargs::OpCodeArg DVMImmediate::asArgument() const {
	return vm::opargs::Immediate{ value };
}

DVMImmediate::DVMImmediate(::u64 value, vm::code::TypeExpr type): value(value), type(type) {}

DVMImmediate DVMImmediate::u8(::u8 value) { return { translateToU64(value), dvm_type::i8() }; }

DVMImmediate DVMImmediate::u16(::u16 value) { return { translateToU64(value), dvm_type::i16() }; }

DVMImmediate DVMImmediate::u32(::u32 value) { return { translateToU64(value), dvm_type::i32() }; }

DVMImmediate DVMImmediate::u64(::u64 value) { return { translateToU64(value), dvm_type::i64() }; }

DVMImmediate DVMImmediate::i8(::i8 value) { return { translateToU64(value), dvm_type::i8() }; }

DVMImmediate DVMImmediate::i16(::i16 value) { return { translateToU64(value), dvm_type::i16() }; }

DVMImmediate DVMImmediate::i32(::i32 value) { return { translateToU64(value), dvm_type::i32() }; }

DVMImmediate DVMImmediate::i64(::i64 value) { return { translateToU64(value), dvm_type::i64() }; }

DVMImmediate DVMImmediate::f32(::f32 value) { return { translateToU64(value), dvm_type::f32() }; }

DVMImmediate DVMImmediate::f64(::f64 value) { return { translateToU64(value), dvm_type::f64() }; }

DVMImmediate DVMImmediate::boolean(bool value) { return { translateToU64(value), dvm_type::i8() }; }

DVMImmediate DVMImmediate::character(char value) {
	return { translateToU64(value), dvm_type::i8() };
}

DVMImmediate::operator vm::opargs::OpCodeArg() const { return asArgument(); }

[[nodiscard]] vm::opargs::OpCodeArg DVMLabel::asArgument() const {
	return vm::opargs::Label{ name };
}

[[nodiscard]] vm::opargs::OpCodeArg DVMFunctionName::asArgument() const {
	if (vm::builtins::isBuiltinFunction(name))
		return vm::opargs::BuiltinFunctionName{ name };
	else
		return vm::opargs::FunctionName{ name };
}

[[nodiscard]] vm::opargs::OpCodeArg DVMExternCFunctionName::asArgument() const {
	return vm::opargs::ExtCFunctionName{ name };
}

[[nodiscard]] vm::opargs::OpCodeArg DVMFFIFunctionName::asArgument() const {
	return vm::opargs::FFIFunctionName{ name };
}

DVMValue::operator vm::opargs::OpCodeArg() const { return asArgument(); }

[[nodiscard]] vm::code::TypeExpr DVMValue::getTypeExpr() const {
	variant_match(stored_value) {
		variant_case(DVMPlace, place) return place.getTypeExpr();
		variant_case(DVMImmediate, immediate) return immediate.getTypeExpr();
		variant_default { CORE_PANIC("This DVM value has no type"); }
	}
	CORE_UNREACHABLE();
}
