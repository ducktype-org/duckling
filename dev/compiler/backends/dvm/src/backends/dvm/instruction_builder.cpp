#include "builders.hpp"

#include <backends/dvm/utils.hpp>
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <vm/program/opcode_args.hpp>

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
#include <vm/program/opcodes_list.hpp>
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
#include <vm/program/opcodes_list.hpp>
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
#include <vm/program/opcodes_list.hpp>
			};

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS
#undef MAKE_LINK

		void buildOpName(const vm::opargs::OpCodeArg& arg, std::stringstream& out) {
			using namespace vm::opargs;
			variant_match(arg) {
				variant_case_novalue(Immediate) out << "imm";
				variant_case_novalue(StackLocalI8) out << "l8";
				variant_case_novalue(StackLocalI16) out << "l16";
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


	}
}

void compiler::backend_vm::InstructionBuilder::setKind(OpKind kind) {
	this->kind = kind;
	kind_set   = true;
}

std::deque<compiler::backend_vm::VmInstruction>
	compiler::backend_vm::InstructionBuilder::build() const {
	CORE_ASSERT(kind_set, "InstructionBuilder::build: kind_set = false");

	std::deque<compiler::backend_vm::VmInstruction> result;

	std::deque new_args = args;

	// Transforms arguments.
	if (kind == OpKind::add || kind == OpKind::sub || kind == OpKind::mul || kind == OpKind::div
	    || kind == OpKind::mod) {
		if (new_args.size() == 3) {
			if (utils::areArgsEqual(new_args[0], new_args[1])) {
				// This resolves e.g. `a = a + b;` by doing `a = b`
				new_args.pop_front();
			} else {
				// This resolves e.g. `a = b + c;`
				// by splitting it into two instructions:
				// a = b;
				// a += c;
				InstructionBuilder instr_mov;

				instr_mov.setKind(OpKind::mov);
				instr_mov.pushArg(new_args[0]);
				instr_mov.pushArg(new_args[1]);

				auto built = instr_mov.build();
				result.insert(result.end(), built.begin(), built.end());
				new_args.pop_front();
				new_args.pop_front();
				new_args.push_front(args.front());
			}
		}
	} else if (kind == OpKind::neg) {
		if (new_args.size() == 2) {
			InstructionBuilder instr_mov;

			instr_mov.setKind(OpKind::mov);
			instr_mov.pushArg(new_args[0]);
			instr_mov.pushArg(new_args[1]);

			auto built = instr_mov.build();
			result.insert(result.end(), built.begin(), built.end());

			new_args.pop_back();
		}
	} else if (kind == OpKind::load || kind == OpKind::store) {
		InstructionBuilder load_or_store_instr;
		load_or_store_instr.kind = kind;
		load_or_store_instr.pushArg(new_args[0]);
		load_or_store_instr.pushArg(new_args[1]);
		auto built = load_or_store_instr.build();
		result.insert(result.end(), built.begin(), built.end());

		InstructionBuilder ext;
		ext.kind = OpKind::ext;
		ext.pushArg(new_args[2]);
		auto built2 = load_or_store_instr.build();
		result.insert(result.end(), built2.begin(), built2.end());
		return result;
	}

	CORE_ASSERT(new_args.size() <= 2, "Cannot handle more than 2 args here");

	usize             arg_count = new_args.size();
	std::stringstream name_stream;
	buildOpName(kind, name_stream);
	for (usize i = 0; i < arg_count; i++) {
		name_stream << "_";
		buildOpName(new_args[i], name_stream);
	}

	std::string opcode_name = name_stream.str();

	if (arg_count == 0)
		result.emplace_back(OPCODE_TO_0_ARGS_FACTORY.at(opcode_name)());
	else if (arg_count == 1)
		result.emplace_back(OPCODE_TO_1_ARGS_FACTORY.at(opcode_name)(new_args[0]));
	else if (arg_count == 2)
		result.emplace_back(OPCODE_TO_2_ARGS_FACTORY.at(opcode_name)(new_args[0], new_args[1]));
	else
		CORE_PANIC("Opcode: ", opcode_name, " does not exist!");

	CORE_ASSERT(!result.empty(), "No instructions were created.");
	return result;
}
