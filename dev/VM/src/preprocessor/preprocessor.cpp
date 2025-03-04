#include "preprocessor.hpp"
#include <core/process/vmprocess.hpp>
#include "code_data/program.hpp"
#include "diagnostic/logger.hpp"
#include "parser/parser.hpp"
#include <code_data/opcode_args.hpp>
#include <code_data/opcodes.hpp>
#include <expected>
#include <vector>
#include "parser/elements.hpp"
#include "parser/errors.hpp"
#include "validator/validator.hpp"

namespace vm {
	namespace {
		void assertTailcallsSignatures(base::StableVector<FuncData>& functions) {
			// void assertTailcallsSignatures(std::vector<FuncData>& functions) {
			for (const auto& func: functions) {
				for (const auto& op: func->bc) {
#ifdef USE_TAIL_CALLS
					if (op.opfun
					    == vm::OpFuns::OPFUNS.at(static_cast<uint16_t>(vm::OpcodeFix8::ret_tailcall)
					    )) {
#else
					if (static_cast<vm::OpcodeFix8>(op.opcode) == vm::OpcodeFix8::ret_tailcall) {
#endif
						CORE_ASSERT(
							func->arg_size == func->next_arg_size,
							"Invalid Tailcall! Caller signature must have arg_size "
							"equal "
							"to "
							"next_arg_size"
						);
						CORE_ASSERT(
							functions[op.arg0].arg_size == functions[op.arg0].next_arg_size,
							"Invalid Tailcall! Called function signature must have "
							"arg_size == "
							"next_arg_size"
						);
						CORE_ASSERT(
							func->arg_size == functions[op.arg0].arg_size,
							"Invalid Tailcall! Caller and called arg size unmatched"
						);
						CORE_ASSERT(
							func->stack_size == functions[op.arg0].stack_size,
							"Invalid Tailcall! Caller and called stack size unmatched"
						);
						CORE_ASSERT(
							func->ret_size == functions[op.arg0].ret_size,
							"Invalid Tailcall! Caller and called ret size unmatched"
						);
					}
				}
			}
		}

		i64 getOpCodeArgValue(
			const std::vector<Box<parser::Func>>& functions,
			Ref<vm::TypeMetadata>                 types,
			CRef<parser::Func>                    current_func,
			usize                                 instruction_index,
			const parser::OpCodeArgAndPosition&   opcode_arg,
			dia::Logger&                          log
		) {
			variant_match(opcode_arg.arg) {
				variant_case(vm::opargs::ImmediateI64, imm) return imm.value;
				variant_case(vm::opargs::StackOffset, offset) return offset.offset;
				variant_case(vm::opargs::ArgsOffset, offset) return offset.offset;
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
						if (functions[i]->name == func.function_name) return i;
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
				return static_cast<u16>(vm::STR_TO_OPCODE_FIX8.at(str.str()));
			} catch (std::out_of_range& err) {
				// @TODO: better errors
				CORE_PANIC(base::strConcat("Incorrect opcode: ", str));
				return 0;
			}
		}

		vm::FuncData changeFuncToFuncData(
			const std::vector<Box<parser::Func>>& functions,
			CRef<parser::Func>                    func,
			Ref<vm::TypeMetadata>                 types,
			dia::Logger&                          log
		) {
			vm::FuncData func_data;
			func_data.name          = func->name.value;
			func_data.ret_size      = 0;
			func_data.arg_size      = func->arg_size;
			func_data.next_arg_size = func->next_arg_size;
			func_data.stack_size    = func->local_size;
			func_data.ret_size      = func->ret_size;

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

		// @TODO: this function returns errors as string, in the future `StreamPrinter`
		// like object should be returned, that can produce both human readable and json
		// error output
		base::Optional<vm::VMProgram>
			getCode(Ref<parser::ParsedProgram> parsed_program, dia::Logger& log) {
			std::vector<vm::FuncData> converted_functions;
			converted_functions.reserve(parsed_program->functions.size());

			for (auto& func: parsed_program->functions) {
				auto converted_func = changeFuncToFuncData(
					parsed_program->functions,
					func.ref(),
					parsed_program->type_metadata.refMut(),
					log
				);
				converted_functions.push_back(converted_func);

				// bool res = program.addFunction(func->name, converted_func);
				// @ TODO: This should be moved to Validator
				// if (!res) {  // Duplicate function name
				// log.log(makeBox<vm::parser::DuplicateFunctionDeclarationError>(
				// parsed_program->position
				// ));
				// }
			}

			vm::VMProgram program(converted_functions, std::move(parsed_program->type_metadata));

			// @TODO: This should be moved to Validator
			assertTailcallsSignatures(program.functions);

			return program;
		}

		std::expected<vm::VMProgram, std::string>
			changeParsedProgramToVMProgram(Ref<parser::ParsedProgram> parsed_program) {
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

std::expected<vm::VMProgram, std::string> vm::Preprocessor::getProgram(const fs::FilePath& file) {
	return getProgram(std::vector{ file });
}

std::expected<vm::VMProgram, std::string>
	vm::Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
	auto maybe_parsed_program = parser::assemble(files);
	if (!maybe_parsed_program) return std::unexpected(maybe_parsed_program.error());

	auto is_valid = validator.validateProgram(maybe_parsed_program.value());
	if (!is_valid) return std::unexpected(is_valid.error());

	auto program = vm::changeParsedProgramToVMProgram(&*maybe_parsed_program);
	if (!program) return std::unexpected(program.error());

	return program;
}

vm::Preprocessor::Preprocessor(VMProcess& process, bool validate_program):
	  validate_program(validate_program) {}
