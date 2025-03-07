#include "utils.hpp"
#include "base/exceptions.hpp"
#include "code_data/opcode_args.hpp"

#include <base/variant.hpp>

bool compiler::backend_vm::utils::areArgsEqual(
	const vm::opargs::OpCodeArg& arg0, const vm::opargs::OpCodeArg& arg1
) {
	using namespace vm::opargs;
	if (arg0.index() != arg1.index()) return false;
	variant_match(arg0) {
		variant_case(Immediate, imm0) { return imm0.value == std::get<Immediate>(arg1).value; }
		variant_case(StackLocalI32, l32) {
			return l32.offset == std::get<StackLocalI32>(arg1).offset;
		}
		variant_case(StackLocalI64, l64) {
			return l64.offset == std::get<StackLocalI64>(arg1).offset;
		}
		variant_case(StackLocalPtr, lptr) {
			return lptr.offset == std::get<StackLocalPtr>(arg1).offset;
		}
		variant_case(ArgsOffset, arg) { return arg.offset == std::get<ArgsOffset>(arg1).offset; }
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
		return compiler::backend_vm::utils::areArgsEqual(instr0.arg0, instr1.arg0);
	}

	template<class T>
	bool cmp2Args(const T& instr0, const T& instr1) {
		return compiler::backend_vm::utils::areArgsEqual(instr0.arg0, instr1.arg0)
		    && compiler::backend_vm::utils::areArgsEqual(instr0.arg1, instr1.arg1);
	}

}

bool compiler::backend_vm::utils::areInstrEqual(
	const VmInstruction& instr0, const VmInstruction& instr1
) {
	if (instr0.index() != instr1.index()) return false;

#define HANDLE_OPCODE_0ARGS(opcode) \
	variant_case(Op_##opcode, op0) return cmp0Args(op0, std::get<Op_##opcode>(instr1));
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type) \
	variant_case(Op_##opcode, op0) return cmp1Args(op0, std::get<Op_##opcode>(instr1));
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type) \
	variant_case(Op_##opcode, op0) return cmp2Args(op0, std::get<Op_##opcode>(instr1));

	variant_match(instr0) {
#include <code_data/opcodes_list.hpp>
	}

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

	CORE_UNREACHABLE();
}
