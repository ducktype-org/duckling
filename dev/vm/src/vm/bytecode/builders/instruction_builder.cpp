#include "builders.hpp"

#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/opcode_args.hpp>

#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace vm::code::builders {
	namespace {
		/**
		 * @brief Creates code instruction with 2 arguments.
		 */
		template<class T, class Arg0Tp, class Arg1Tp>
		Instruction makeVmOpcode2Args(vm::opargs::OpCodeArg arg0, vm::opargs::OpCodeArg arg1) {
			CORE_ASSERT(std::holds_alternative<Arg0Tp>(arg0), "Invalid arg0 for opcode");
			CORE_ASSERT(std::holds_alternative<Arg1Tp>(arg1), "Invalid arg1 for opcode");
			return T{ std::get<Arg0Tp>(arg0), std::get<Arg1Tp>(arg1) };
		}

		/**
		 * @brief Creates code instruction with 1 argument.
		 */
		template<class T, class Arg0Tp>
		Instruction makeVmOpcode1Args(vm::opargs::OpCodeArg arg0) {
			CORE_ASSERT(std::holds_alternative<Arg0Tp>(arg0), "Invalid arg0 for opcode");
			return T{ std::get<Arg0Tp>(arg0) };
		}

		/**
		 * @brief Creates code instruction with no arguments.
		 */
		template<class T>
		Instruction makeVmOpcode0Args() {
			return T{};
		}

#define MAKE_LINK(opcode, func) std::make_pair(std::string(#opcode), func),

#define HANDLE_OPCODE_0ARGS(opcode) MAKE_LINK(opcode, makeVmOpcode0Args<VM_INSTR_FROM_NAME(opcode)>)
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)

		const std::unordered_map OPCODE_TO_0_ARGS_FACTORY = {
#include <vm/bytecode/opcode_definitions.hpp>
		};

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

#define HANDLE_OPCODE_0ARGS(opcode)
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type) \
	MAKE_LINK(opcode, makeVmOpcode1Args<VM_INSTR_FROM_NAME(opcode) COMMA arg0_type>)
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)

		const std::unordered_map OPCODE_TO_1_ARGS_FACTORY = {
#include <vm/bytecode/opcode_definitions.hpp>
		};

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

#define HANDLE_OPCODE_0ARGS(opcode)
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type) \
	MAKE_LINK(opcode, makeVmOpcode2Args<VM_INSTR_FROM_NAME(opcode) COMMA arg0_type COMMA arg1_type>)

		const std::unordered_map OPCODE_TO_2_ARGS_FACTORY = {
#include <vm/bytecode/opcode_definitions.hpp>
		};

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS
#undef MAKE_LINK

		/**
		 * @brief Appends opcode argument as string to a to a stream.
		 */
		void pushOpcodeArg(const opargs::OpCodeArg& arg, std::ostream& out) {
			using namespace opargs;
			variant_match(arg) {
				variant_case_novalue(Immediate) out << "imm";
				variant_case_novalue(StackLocalI8) out << "l8";
				variant_case_novalue(StackLocalI16) out << "l16";
				variant_case_novalue(StackLocalI32) out << "l32";
				variant_case_novalue(StackLocalI64) out << "l64";
				variant_case_novalue(StackLocalAny) out << "any";
				variant_case_novalue(StackLocalPtr) out << "lptr";
				variant_case_novalue(opargs::Type) out << "type";
				variant_case_novalue(FunctionName) out << "func";
				variant_case_novalue(Label) out << "label";
				variant_default CORE_PANIC("Unhandled arg type during opcode generation.");
			}
		}

		/**
		 * @brief Appends opcode kind as string to a to a stream.
		 */
		void pushOpcodeKind(const OpKind& kind, std::stringstream& out) {
			out << base::enumToStr(kind).strView();
		}
	}
}

void vm::code::builders::InstructionBuilder::pushArg(const vm::opargs::OpCodeArg& arg) {
	args.push_back(arg);
}

vm::code::builders::InstructionBuilder::InstructionBuilder(OpKind kind) { setKind(kind); }

void vm::code::builders::InstructionBuilder::setKind(OpKind kind) {
	this->kind = kind;
	kind_set   = true;
}

std::vector<vm::code::Instruction> vm::code::builders::InstructionBuilder::build() const {
	CORE_ASSERT(kind_set, "InstructionBuilder::build: kind_set = false");

	std::vector<vm::code::Instruction> result;

	std::vector new_args = args;

	// Transforms arguments.
	if ((kind == OpKind::load || kind == OpKind::store) && args.size() == 3) {
		InstructionBuilder load_or_store_instr(kind);
		load_or_store_instr.pushArgs(new_args[0], new_args[1]);
		auto built = load_or_store_instr.build();
		result.insert(result.end(), built.begin(), built.end());

		InstructionBuilder ext(OpKind::ext);
		ext.pushArg(new_args[2]);
		auto built2 = load_or_store_instr.build();
		result.insert(result.end(), built2.begin(), built2.end());
		return { result.begin(), result.end() };
	}

	CORE_ASSERT(new_args.size() <= 2, "Cannot handle more than 2 args here");

	usize             arg_count = new_args.size();
	std::stringstream name_stream;
	pushOpcodeKind(kind, name_stream);
	for (usize i = 0; i < arg_count; i++) {
		name_stream << "_";
		pushOpcodeArg(new_args[i], name_stream);
	}

	std::string opcode_name = name_stream.str();

	try {
		if (arg_count == 0)
			result.emplace_back(OPCODE_TO_0_ARGS_FACTORY.at(opcode_name)());
		else if (arg_count == 1)
			result.emplace_back(OPCODE_TO_1_ARGS_FACTORY.at(opcode_name)(new_args[0]));
		else
			result.emplace_back(OPCODE_TO_2_ARGS_FACTORY.at(opcode_name)(new_args[0], new_args[1]));
	} catch (std::out_of_range&) { CORE_PANIC("Opcode: ", opcode_name, " does not exist!"); }

	CORE_ASSERT(!result.empty(), "No instructions were created.");
	return { result.begin(), result.end() };
}
