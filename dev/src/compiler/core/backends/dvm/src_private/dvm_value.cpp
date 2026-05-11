#include "dvm_value.hpp"

#include "common.hpp"

#include <vm/bytecode/opcode_args.hpp>
#include <vm/core/builtin_functions.hpp>
#include <vm/utils/interpret.hpp>

using namespace compiler::backend_vm::internal;

[[nodiscard]] vm::opargs::OpCodeArg DVMPlace::asArgument() const {
	variant_match(type) {
		variant_case(vm::code::PrimitiveType, primitive) {
			if (primitive.size == Bytes{ 8 }) return vm::opargs::Place64{ name };
			if (primitive.size == Bytes{ 4 }) return vm::opargs::Place32{ name };
			if (primitive.size == Bytes{ 2 }) return vm::opargs::Place16{ name };
			if (primitive.size == Bytes{ 1 }) return vm::opargs::Place8{ name };
		}
		variant_case(vm::code::PointerType, pointer) { return vm::opargs::PlacePtr(name); }
		variant_case(vm::code::DataType, data) { return vm::opargs::PlaceStructure(name); }
		variant_case(vm::code::OpaqueType, opaque) { return vm::opargs::PlaceOpq(name); }
		variant_default {
			CORE_PANIC("DVMLocal type not supported for argument: ", typeName(type));
		}
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

const vm::code::TypeOfData& DVMPlace::getType() const { return type; }

[[nodiscard]] vm::opargs::OpCodeArg DVMImmediate::asArgument() const {
	return vm::opargs::Immediate{ value };
}

DVMImmediate::DVMImmediate(::u64 value, vm::code::TypeOfData type):
	  value(value),
	  type(std::move(type)) {}

DVMImmediate DVMImmediate::u8(::u8 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i8"), Bytes{ 1 }) };
}

DVMImmediate DVMImmediate::u16(::u16 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i16"), Bytes{ 2 }) };
}

DVMImmediate DVMImmediate::u32(::u32 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i32"), Bytes{ 4 }) };
}

DVMImmediate DVMImmediate::u64(::u64 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i64"), Bytes{ 8 }) };
}

DVMImmediate DVMImmediate::i8(::i8 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i8"), Bytes{ 1 }) };
}

DVMImmediate DVMImmediate::i16(::i16 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i16"), Bytes{ 2 }) };
}

DVMImmediate DVMImmediate::i32(::i32 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i32"), Bytes{ 4 }) };
}

DVMImmediate DVMImmediate::i64(::i64 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i64"), Bytes{ 8 }) };
}

DVMImmediate DVMImmediate::f32(::f32 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("f32"), Bytes{ 4 }) };
}

DVMImmediate DVMImmediate::f64(::f64 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("f64"), Bytes{ 8 }) };
}

DVMImmediate DVMImmediate::boolean(bool value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i8"), Bytes{ 1 }) };
}

DVMImmediate DVMImmediate::character(char value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i8"), Bytes{ 1 }) };
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

DVMValue::operator vm::opargs::OpCodeArg() const { return asArgument(); }

[[nodiscard]] vm::code::TypeOfData DVMValue::getType() const {
	variant_match(stored_value) {
		variant_case(DVMPlace, place) return place.getType();
		variant_case(DVMImmediate, imm) return imm.type;
		variant_default CORE_PANIC("Tried to get type of label or function literal");
	}
}
