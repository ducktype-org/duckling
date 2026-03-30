#include "dvm_value.hpp"

#include <vm/bytecode/opcode_args.hpp>
#include <vm/core/process/builtin_functions.hpp>
#include <vm/utils/interpret.hpp>

using namespace compiler::backend_vm::internal;

[[nodiscard]] vm::opargs::OpCodeArg DVMLocal::asArgument() const {
	variant_match(type) {
		variant_case(vm::code::PrimitiveType, primitive) {
			if (primitive.size == 8) return vm::opargs::StackLocal64{ name };
			if (primitive.size == 4) return vm::opargs::StackLocal32{ name };
			if (primitive.size == 2) return vm::opargs::StackLocal16{ name };
			if (primitive.size == 1) return vm::opargs::StackLocal8{ name };
		}
		variant_case(vm::code::PointerType, pointer) { return vm::opargs::StackLocalPtr(name); }
		variant_case(vm::code::OpaqueType, opaque) { return vm::opargs::StackLocalOpq(name); }
		variant_default {
			CORE_PANIC("DVMLocal type not supported for argument: ", typeName(type));
		}
	}
	CORE_UNREACHABLE();
}

[[nodiscard]] vm::opargs::OpCodeArg DVMLocal::asAnyArgument() const {
	return vm::opargs::StackLocalAny{ name };
}

DVMLocal::operator vm::opargs::OpCodeArg() const { return asArgument(); }

[[nodiscard]] vm::opargs::OpCodeArg DVMGlobal::asArgument() const {
	variant_match(type) {
		variant_case(vm::code::PrimitiveType, primitive) {
			if (primitive.size == 8) return vm::opargs::Global64{ name };
			if (primitive.size == 4) return vm::opargs::Global32{ name };
			if (primitive.size == 2) return vm::opargs::Global16{ name };
			if (primitive.size == 1) return vm::opargs::Global8{ name };
		}
		variant_case(vm::code::PointerType, pointer) { return vm::opargs::GlobalPtr(name); }
		variant_case(vm::code::OpaqueType, opaque) { return vm::opargs::GlobalOpq(name); }
		variant_default {
			CORE_PANIC("DVMGlobal type not supported for argument: ", typeName(type));
		}
	}
	CORE_UNREACHABLE();
}

DVMGlobal::operator vm::opargs::OpCodeArg() const { return asArgument(); }

[[nodiscard]] vm::opargs::OpCodeArg DVMGlobal::asAnyArgument() const {
	return vm::opargs::GlobalAny{ name };
}

[[nodiscard]] vm::opargs::OpCodeArg DVMValue::asArgument() const {
	return VISIT(stored_value, value, return value.asArgument());
}

[[nodiscard]] vm::opargs::OpCodeArg DVMValue::asAnyArgument() const {
	variant_match(stored_value) {
		variant_case(DVMPlace, place) return place.asAnyArgument();
		variant_default CORE_PANIC("Attempted to convert a non-anyable value to any argument");
	}
}

[[nodiscard]] vm::code::TypeOfData DVMPlace::getType() const {
	return VISIT(stored_place, place, return place.type);
}

[[nodiscard]] vm::opargs::OpCodeArg DVMPlace::asArgument() const {
	return VISIT(stored_place, place, return place.asArgument());
}

[[nodiscard]] vm::opargs::OpCodeArg DVMPlace::asAnyArgument() const {
	return VISIT(stored_place, place, return place.asAnyArgument());
}

[[nodiscard]] vm::opargs::OpCodeArg DVMImmediate::asArgument() const {
	return vm::opargs::Immediate{ value };
}

DVMPlace::operator vm::opargs::OpCodeArg() const { return asArgument(); }

namespace {
	template<class T>
	u64 translateToU64(T value) {
		if constexpr (sizeof(T) == 8)
			return vm::safeReadObjectBytes<u64>(value);
		else if constexpr (sizeof(T) == 4)
			return vm::safeReadObjectBytes<u32>(value);
		else if constexpr (sizeof(T) == 2)
			return vm::safeReadObjectBytes<u16>(value);
		else if constexpr (sizeof(T) == 1)
			return static_cast<u64>(vm::safeReadObjectBytes<u8>(value));
		else
			CORE_PANIC("Unsupported immediate size: ", sizeof(T));
	}
}

DVMImmediate::DVMImmediate(u64 value, vm::code::TypeOfData type):
	  value(value),
	  type(std::move(type)) {}

DVMImmediate DVMImmediate::i8(u8 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i8"), 1) };
}

DVMImmediate DVMImmediate::i16(u16 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i16"), 2) };
}

DVMImmediate DVMImmediate::i32(u32 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i32"), 4) };
}

DVMImmediate DVMImmediate::i64(u64 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i64"), 8) };
}

DVMImmediate DVMImmediate::f32(::f32 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("f32"), 4) };
}

DVMImmediate DVMImmediate::f64(::f64 value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("f64"), 8) };
}

DVMImmediate DVMImmediate::boolean(bool value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i8"), 1) };
}

DVMImmediate DVMImmediate::character(char value) {
	return { translateToU64(value), vm::code::PrimitiveType(base::StrID("i8"), 1) };
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
