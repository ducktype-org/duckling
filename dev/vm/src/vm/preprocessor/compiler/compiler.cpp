#include "compiler.hpp"
#include "base/string_id.hpp"
#include "base/variant.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
#include "vm/core/thread/low_program/opcodes.hpp"
#include "vm/preprocessor/parser/elements.hpp"
#include "vm/program/instructions.hpp"
#include "vm/program/opcode_args.hpp"
#include "vm/program/program.hpp"
#include <expected>
#include <type_traits>
#include <variant>

namespace vm {
	namespace {
		i64 getOpCodeArgValue(
			const base::HashMap<base::StrID, program::Function>& functions,
			vm::TypeMetadata&                                    types,
			const program::Function&                             current_func,
			usize                                                instruction_index,
			const vm::opargs::OpCodeArg&                         opcode_arg,
			dia::Logger&                                         log
		) {
			variant_match(opcode_arg) {
				variant_case(vm::opargs::Immediate, imm) return imm.value;

#define HANDLE_OFFSET(Type) variant_case(vm::opargs::Type, offset_type) return offset_type.offset;
				FOR_EACH(HANDLE_OFFSET, VM_OPCODE_OFFSET_TYPES);
#undef HANDLE_OFFSET

				variant_case(vm::opargs::Type, type_arg) {
					auto type_obj = types.getTypeByName(type_arg.type_name);
					if (type_obj)
						return static_cast<i64>(static_cast<u64>(type_obj.value()->getID()));
					log.log(
						makeBox<vm::parser::UnknownType>(opcode_arg.position, type_arg.type_name)
					);
					return 0;
				}
				variant_case(vm::opargs::FunctionName, func) {
					for (i64 i = 0; i < functions.size(); i++)
						if (functions[base::safeIntConv<u64>(i)]->name == func.function_name)
							return i;
					log.log(makeBox<vm::parser::UnknownFunction>(
						opcode_arg.position, func.function_name
					));
					return 0;
				}
				variant_case(vm::opargs::Label, label) {
					auto it = current_func->code->label_position.find(label.label_name);
					if (it != current_func->code->label_position.end()) {
						// We have to calculate the
						// difference instead of absolute jump position,
						// because our instruction counter is a pointer.
						return static_cast<i64>(it->second) - static_cast<i64>(instruction_index)
						     - 1;
					}
					log.log(makeBox<vm::parser::InvalidLabel>(
						opcode_arg.position, "Label does not exist."
					));
					return 0;
				}
			}
			CORE_UNREACHABLE();
		}

		vm::low::FuncData changeFuncToFuncData(
			const base::HashMap<base::StrID, program::Function>& functions,
			const program::Function&                             func,
			vm::TypeMetadata&                                    types,
			dia::Logger&                                         log
		) {
			vm::low::FuncData func_data;
			func_data.name             = func.name;
			func_data.arg_size         = func.arg_size;
			func_data.local_stack_size = func.local_stack_size;
			func_data.ret_size         = func.ret_size;

			for (usize op_idx = 0; op_idx < func.body.size(); op_idx++) {
				const auto& op = func.body[op_idx];
				if (std::holds_alternative<program::instructions::Comment>(op)) continue;
				i64 arg_0 = 0;
				i64 arg_1 = 0;
				variant_match(op) {
#define HANDLE_OPCODE_0ARGS(opcode) \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {}
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                                      \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {                               \
		arg_0 = getOpCodeArgValue(functions, types, func, op_idx, instr.arg0, log); \
	}
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                           \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {                               \
		arg_0 = getOpCodeArgValue(functions, types, func, op_idx, instr.arg0, log); \
		arg_1 = getOpCodeArgValue(functions, types, func, op_idx, instr.arg1, log); \
	}
#include <vm/program/opcodes_list.hpp>
				}

				func_data.bc.emplace_back(vm::Fix8Instruction{
#ifdef USE_TAIL_CALLS
					.opfun = vm::OpFuns::OPFUNS.at(low::fix8FromInstr(op)),
#else
					.opcode = static_cast<u16>(low::fix8FromInstr(op)),
#endif
					.arg0 = static_cast<i32>(arg_0),
					.arg1 = static_cast<i32>(arg_1) });
			}
			return func_data;
		}
	}

	std::expected<low::LowVMProgram, dia::Logger> compiler::compile(program::Program program) {
		std::vector<low::FuncData> converted_functions;
		dia::Logger                log;
		converted_functions.reserve(program.functions.size());

		// also add implementation for program.func_ids and program.type_ids
				// if (opcode->opcode_name == base::StrID("label")) {
				// 	auto arg = opcode->args[0];
				// 	CORE_ASSERT(
				// 		std::holds_alternative<vm::opargs::Label>(arg.arg),
				// 		"Something went wrong during label parsing."
				// 	);
				// 	auto label_name = std::get<vm::opargs::Label>(arg.arg).label_name;

				// 	if (out->label_position.contains(label_name)) {
				// 		auto msg
				// 			= makeBox<vm::parser::InvalidLabel>(arg.position, "Repeated label.");
				// 		for (auto&& lbl: labels)
				// 			if (lbl.first == label_name)
				// 				msg->addNote(makeBox<vm::parser::RepeatedLabelNote>(lbl.second));
				// 		state.log(std::move(msg));
				// 	} else {
				// 		out->label_position.put(label_name, out->opcodes.size());
				// 		labels.emplace_back(label_name, arg.position);
				// 	}
				// } else {

		for (auto& func: program.functions | std::views::values) {
			auto converted_func
				= changeFuncToFuncData(program.functions, func, *program.type_metadata, log);
			converted_functions.push_back(converted_func);
		}

		low::LowVMProgram low_program(converted_functions, std::move(program.type_metadata));

		return low_program;
	}
}
