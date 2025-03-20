#include "compiler.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
#include "vm/preprocessor/parser/elements.hpp"
#include "vm/program/program.hpp"

namespace vm {
	namespace {
		i64 getOpCodeArgValue(
			const std::vector<Box<parser::Func>>& functions,
			Ref<vm::TypeMetadata>                 types,
			CRef<parser::Func>                    current_func,
			usize                                 instruction_index,
			const parser::OpCodeArgAndPosition&   opcode_arg,
			dia::Logger&                          log
		) {
			variant_match(opcode_arg.arg) {
				variant_case(vm::opargs::Immediate, imm) return imm.value;

#define HANDLE_OFFSET(Type) variant_case(vm::opargs::Type, offset_type) return offset_type.offset;
				FOR_EACH(HANDLE_OFFSET, VM_OPCODE_OFFSET_TYPES);
#undef HANDLE_OFFSET

				variant_case(vm::opargs::Type, type_arg) {
					auto type_obj = types->getTypeByName(type_arg.type_name);
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

		u16 nameToOpcodeValue(base::StrID str) {
			try {
				return static_cast<u16>(low::STR_TO_OPCODE_FIX8.at(str.str()));
			} catch (std::out_of_range& err) {
				// @TODO: better errors
				CORE_PANIC(base::strConcat("Incorrect opcode: ", str));
			}
		}

		vm::low::FuncData changeFuncToFuncData(
			const std::vector<program::Function>& functions,
			const program::Function&              func,
			vm::TypeMetadata&                     types,
			dia::Logger&                          log
		) {
			vm::low::FuncData func_data;
			func_data.name       = func.name;
			func_data.arg_size   = func.arg_size;
			func_data.stack_size = func.stack_size;
			func_data.ret_size   = func.ret_size;

			for (usize op_idx = 0; op_idx < func.body->opcodes.size(); op_idx++) {
				auto&& op    = func.code->opcodes[op_idx];
				i64    arg_0 = 0;
				i64    arg_1 = 0;
				switch (op->args.size()) {
				case 0: {
					break;
				}
				case 1: {
					arg_0 = getOpCodeArgValue(functions, types, func, op_idx, op->args[0], log);
					break;
				}
				case 2: {
					arg_0 = getOpCodeArgValue(functions, types, func, op_idx, op->args[0], log);
					arg_1 = getOpCodeArgValue(functions, types, func, op_idx, op->args[1], log);
					break;
				}
				}


#ifdef USE_TAIL_CALLS
				func_data.bc.emplace_back(vm::Fix8Instruction{
					.opfun = vm::OpFuns::OPFUNS.at(nameToOpcodeValue(op->opcode_name)),
					.arg0  = static_cast<i32>(arg_0),
					.arg1  = static_cast<i32>(arg_1) });
#endif

// #else breaks clang-format for some reason (?)
#ifndef USE_TAIL_CALLS
				func_data.bc.emplace_back(vm::Fix8Instruction{
					.opcode = static_cast<u16>(nameToOpcodeValue(op->opcode_name)),
					.arg0   = static_cast<i32>(arg_0),
					.arg1   = static_cast<i32>(arg_1) });
#endif
			}
			return func_data;
		}

		base::Optional<low::LowVMProgram>
			lowerToLowProgram(program::Program& program, dia::Logger& log) {
			std::vector<low::FuncData> converted_functions;
			converted_functions.reserve(program.functions.size());

			for (auto& func: program.functions) {
				auto converted_func = changeFuncToFuncData(
					program.functions, &func, program.type_metadata.refMut(), log
				);
				converted_functions.push_back(converted_func);
			}

			low::LowVMProgram low_program(converted_functions, std::move(program.type_metadata));

			return low_program;
		}

		std::expected<low::LowVMProgram, std::string>
			changeParsedProgramToLowVMProgram(program::Program& program) {
			auto log = dia::Logger();

			auto low_program = lowerToLowProgram(program, log);
			if (!low_program || log.bad()) {
				std::stringstream stream;
				log.dumpLogAndClear(true, stream);
				return std::unexpected(stream.str());
			}

			return std::move(*program);
		}

	}

	std::expected<low::LowVMProgram, dia::Logger>
		compiler::compile(const std::vector<program::CodeFile>& files) {
		auto program = program::Program{
			.functions     = {},
			.types         = {},
			.type_metadata = makeBox<TypeMetadata>(),
		};
	}
}
