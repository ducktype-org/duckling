#include "preprocessor.hpp"

#include <expected>
#include <vector>

#include "parser/elements.hpp"
#include "parser/errors.hpp"
#include "parser/parser.hpp"
#include "validator/validator.hpp"
#include <diagnostic/logger.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/program/opcode_args.hpp>

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
				return static_cast<u16>(vm::low::STR_TO_OPCODE_FIX8.at(str.str()));
			} catch (std::out_of_range& err) {
				// @TODO: better errors
				CORE_PANIC(base::strConcat("Incorrect opcode: ", str));
			}
		}

		vm::low::FuncData changeFuncToFuncData(
			const std::vector<Box<parser::Func>>& functions,
			CRef<parser::Func>                    func,
			Ref<vm::TypeMetadata>                 types,
			dia::Logger&                          log
		) {
			vm::low::FuncData func_data;
			func_data.name             = func->name.value;
			func_data.arg_size         = func->arg_size;
			func_data.local_stack_size = func->local_size;
			func_data.ret_size         = func->ret_size;

			for (usize op_idx = 0; op_idx < func->code->opcodes.size(); op_idx++) {
				auto&& op    = func->code->opcodes[op_idx];
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

		base::Optional<vm::low::LowVMProgram>
			getCode(Ref<parser::ParsedProgram> parsed_program, dia::Logger& log) {
			std::vector<vm::low::FuncData> converted_functions;
			converted_functions.reserve(parsed_program->functions.size());

			for (auto& func: parsed_program->functions) {
				auto converted_func = changeFuncToFuncData(
					parsed_program->functions,
					func.ref(),
					parsed_program->type_metadata.refMut(),
					log
				);
				converted_functions.push_back(converted_func);
			}

			vm::low::LowVMProgram program(
				converted_functions, std::move(parsed_program->type_metadata)
			);

			return program;
		}

		std::expected<vm::low::LowVMProgram, std::string>
			changeParsedProgramTLowVMProgram(Ref<parser::ParsedProgram> parsed_program) {
			auto log = dia::Logger();

			auto program = getCode(parsed_program, log);
			if (!program || log.bad()) {
				std::stringstream stream;
				log.dumpLogAndClear(true, stream);
				return std::unexpected(stream.str());
			}

			return std::move(*program);
		}
	}
}

std::expected<vm::low::LowVMProgram, std::string>
	vm::Preprocessor::getProgram(const fs::FilePath& file) {
	return getProgram(std::vector{ file });
}

std::expected<vm::low::LowVMProgram, std::string>
	vm::Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
	auto maybe_parsed_program = parser::assemble(files);
	if (!maybe_parsed_program) return std::unexpected(maybe_parsed_program.error());

	auto is_valid = validator::verify(maybe_parsed_program.value());
	if (is_valid.has_value()) return std::unexpected(is_valid.value());

	auto program = vm::changeParsedProgramTLowVMProgram(&*maybe_parsed_program);
	if (!program) return std::unexpected(program.error());

	return program;
}

vm::Preprocessor::Preprocessor(bool validate_program): validate_program(validate_program) {}
