#include "utils.hpp"

#include <base/exceptions.hpp>
#include <base/for_each.hpp>
#include <base/variant.hpp>

#include <vm/code/opcode_args.hpp>

bool vm::code::utils::areArgsEqual(
	const vm::opargs::OpCodeArg& arg0, const vm::opargs::OpCodeArg& arg1
) {
	using namespace vm::opargs;
	if (arg0.index() != arg1.index()) return false;
	variant_match(arg0) {
		variant_case(Immediate, imm0) { return imm0.value == std::get<Immediate>(arg1).value; }

#define HANDLE_OFFSET(Type) \
	variant_case(Type, offset) { return offset.offset == std::get<Type>(arg1).offset; }

		FOR_EACH(HANDLE_OFFSET, VM_OPARG_OFFSET_TYPES);

		variant_case(Type, tp) { return tp.type_name == std::get<Type>(arg1).type_name; }
		variant_case(FunctionName, func) {
			return func.function_name == std::get<FunctionName>(arg1).function_name;
		}

		variant_case(Label, label) { return label.label_name == std::get<Label>(arg1).label_name; }
		variant_default CORE_PANIC("Unhandled arg type opcode arg comparision (==).");
	}
	CORE_UNREACHABLE();
}

namespace {
	template<class T>
	bool cmp0Args(const T&, const T&) {
		return true;
	}

	template<class T>
	bool cmp1Args(const T& instr0, const T& instr1) {
		return vm::code::utils::areArgsEqual(instr0.arg0, instr1.arg0);
	}

	template<class T>
	bool cmp2Args(const T& instr0, const T& instr1) {
		return vm::code::utils::areArgsEqual(instr0.arg0, instr1.arg0)
		    && vm::code::utils::areArgsEqual(instr0.arg1, instr1.arg1);
	}

}

bool vm::code::utils::areInstrEqual(const Instruction& instr0, const Instruction& instr1) {
	if (instr0.index() != instr1.index()) return false;

#define HANDLE_OPCODE_0ARGS(opcode)                                \
	variant_case(VM_INSTR_FROM_NAME(opcode), op0) return cmp0Args( \
		op0, std::get<VM_INSTR_FROM_NAME(opcode)>(instr1)          \
	);
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                     \
	variant_case(VM_INSTR_FROM_NAME(opcode), op0) return cmp1Args( \
		op0, std::get<VM_INSTR_FROM_NAME(opcode)>(instr1)          \
	);
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)          \
	variant_case(VM_INSTR_FROM_NAME(opcode), op0) return cmp2Args( \
		op0, std::get<VM_INSTR_FROM_NAME(opcode)>(instr1)          \
	);

	variant_match(instr0) {
#include <vm/code/opcodes_list.hpp>
	}

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

	CORE_UNREACHABLE();
}

bool vm::code::utils::areTypesEqual(const TypeOfData& type0, const TypeOfData& type1) {
	if (type0.index() != type1.index()) return false;

	return std::visit(
		[&]<class T>(const T& t0) {
			const auto& t1 = std::get<T>(type1);
			return t0 == t1;
		},
		type0
	);
}
