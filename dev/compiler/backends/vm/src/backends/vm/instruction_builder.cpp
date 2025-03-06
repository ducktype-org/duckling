#include "base/exceptions.hpp"
#include "base/stringifyable_enum.hpp"
#include "base/variant.hpp"
#include "builders.hpp"
#include "code_data/opcode_args.hpp"

namespace compiler::backend_vm {
	namespace {
		template<class T, class Arg0Tp, class Arg1Tp>
		compiler::backend_vm::VmInstruction
			makeVmOpcode2Args(vm::opargs::OpCodeArg arg0, vm::opargs::OpCodeArg arg1) {
			CORE_ASSERT(std::holds_alternative<Arg0Tp>(arg0), "Invalid arg0 for opcode");
			CORE_ASSERT(std::holds_alternative<Arg1Tp>(arg1), "Invalid arg1 for opcode");
			return T{ std::get<Arg0Tp>(arg0), std::get<Arg1Tp>(arg1) };
		}

		template<class T, class Arg0Tp>
		compiler::backend_vm::VmInstruction makeVmOpcode1Args(vm::opargs::OpCodeArg arg0) {
			CORE_ASSERT(std::holds_alternative<Arg0Tp>(arg0), "Invalid arg0 for opcode");
			return T{ std::get<Arg0Tp>(arg0) };
		}

		template<class T>
		compiler::backend_vm::VmInstruction makeVmOpcode0Args() {
			return T{};
		}

#define MAKE_LINK(opcode, func) std::make_pair(std::string(#opcode), func),

#define HANDLE_OPCODE_0ARGS(opcode) MAKE_LINK(opcode, makeVmOpcode0Args<Op_##opcode>)
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)

		const std::unordered_map<std::string, VmInstruction (*)()> OPCODE_TO_0_ARGS_FACTORY = {
#include <code_data/opcodes_list.hpp>
		};

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

#define HANDLE_OPCODE_0ARGS(opcode)
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type) \
	MAKE_LINK(opcode, makeVmOpcode1Args<Op_##opcode COMMA arg0_type>)
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)

		const std::unordered_map<std::string, VmInstruction (*)(vm::opargs::OpCodeArg)>
			OPCODE_TO_1_ARGS_FACTORY = {
#include <code_data/opcodes_list.hpp>
			};

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

#define HANDLE_OPCODE_0ARGS(opcode)
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type) \
	MAKE_LINK(opcode, makeVmOpcode2Args<Op_##opcode COMMA arg0_type COMMA arg1_type>)

		const std::unordered_map<
			std::string,
			VmInstruction (*)(vm::opargs::OpCodeArg, vm::opargs::OpCodeArg)>
			OPCODE_TO_2_ARGS_FACTORY = {
#include <code_data/opcodes_list.hpp>
			};

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS
#undef MAKE_LINK

		void buildOpName(const vm::opargs::OpCodeArg& arg, std::stringstream& out) {
			using namespace vm::opargs;
			variant_match(arg) {
				variant_case_novalue(Immediate) out << "imm";
				variant_case_novalue(StackLocalI32) out << "l32";
				variant_case_novalue(StackLocalI64) out << "l64";
				variant_case_novalue(StackLocalPtr) out << "lptr";
				variant_case_novalue(ArgsOffset) out << "arg64";
				variant_case_novalue(Type) out << "type";
				variant_case_novalue(FunctionName) out << "func";
				variant_case_novalue(Label) out << "label";
				variant_default CORE_PANIC("Unhandled arg type during opcode generation.");
			}
		}

		void buildOpName(const OpKind& kind, std::stringstream& out) {
			out << base::enumToStr(kind).strView();
		}

		bool areEqual(const vm::opargs::OpCodeArg& arg0, const vm::opargs::OpCodeArg& arg1) {
			using namespace vm::opargs;
			if (arg0.index() != arg1.index()) return false;
			variant_match(arg0) {
				variant_case(Immediate, imm0) {}
				variant_case(StackLocalI32, l32) {
					return l32.offset == std::get<StackLocalI32>(arg1).offset;
				}
				variant_case(StackLocalI64, l64) {
					return l64.offset == std::get<StackLocalI64>(arg1).offset;
				}
				variant_case(StackLocalPtr, lptr) {
					return lptr.offset == std::get<StackLocalPtr>(arg1).offset;
				}
				variant_case(ArgsOffset, arg) {
					return arg.offset == std::get<ArgsOffset>(arg1).offset;
				}
				variant_case(Type, tp) { return tp.type_name == std::get<Type>(arg1).type_name; }
				variant_case(FunctionName, func) {
					return func.function_name == std::get<FunctionName>(arg1).function_name;
				}

				variant_case(Label, label) {
					return label.label_name == std::get<Label>(arg1).label_name;
				}
				variant_default CORE_PANIC("Unhandled arg type opcode arg comparision (==).");
			}
			CORE_UNREACHABLE();
		}
	}
}

void compiler::backend_vm::InstructionBuilder::setKind(OpKind kind) {
	this->kind = kind;
	kind_set   = true;
}

compiler::backend_vm::VmInstruction compiler::backend_vm::InstructionBuilder::build() const {
	CORE_ASSERT(kind_set, "InstructionBuilder::build: kind_set = false");

	std::deque real_args = args;
	if (real_args.size() == 3) {
		if (areEqual(real_args[0], real_args[1])) real_args.pop_front();
	}

	usize             arg_count = real_args.size();
	std::stringstream name_stream;
	buildOpName(kind, name_stream);
	for (usize i = 0; i < arg_count; i++) {
		name_stream << "_";
		buildOpName(real_args[i], name_stream);
	}

	std::string opcode_name = name_stream.str();
	std::cerr << opcode_name << '\n';

	switch (arg_count) {
	case 0: {
		return OPCODE_TO_0_ARGS_FACTORY.at(opcode_name)();
	}
	case 1: {
		return OPCODE_TO_1_ARGS_FACTORY.at(opcode_name)(real_args[0]);
	}
	case 2: {
		return OPCODE_TO_2_ARGS_FACTORY.at(opcode_name)(real_args[0], real_args[1]);
	}
	default:
		std::cerr << base::strConcat("Opcode: ", opcode_name, " does not exist!\n");
		return Op_nop{};
		// CORE_PANIC("Opcode: ", opcode_name, " does not exist!");
	}
	CORE_UNREACHABLE();
}
