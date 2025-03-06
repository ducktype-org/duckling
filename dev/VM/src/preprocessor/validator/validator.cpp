#include "validator.hpp"
#include "base/box.hpp"
#include "base/exceptions.hpp"
#include "base/variant.hpp"
#include "code_data/opcode_args.hpp"
#include "errors.hpp"
#include "preprocessor/parser/elements.hpp"
#include "preprocessor/preprocessor.hpp"
#include <expected>
#include <sstream>

namespace vm::validator {
	std::expected<bool, std::string> Validator::validateProgram(const parser::ParsedProgram& program
	) {
		auto log = dia::Logger();

		validateMainExistance(program, log);
		validateTailcallSignatures(program, log);
		validateDuplicateFunctionDeclarations(program, log);
		
		if (log.bad()) {
			std::stringstream stream;
			log.dumpLogAndClear(true, stream);
			return std::unexpected(stream.str());
		}
		return true;
	}

	void Validator::validateMainExistance(const parser::ParsedProgram& program, dia::Logger& log) {
		bool main_found = false;
		for (auto& func: program.functions) {
			if (func->name.value.strView() == "main") {
				main_found = true;
				break;
			}
		}

		if (!main_found)
			// @TODO: Change that to real position
			log.log(makeBox<vm::validator::NoMainError>(dia::SourcePosition::fakePosition()));
	}

	void Validator::validateTailcallSignatures(
		const parser::ParsedProgram& program, dia::Logger& log
	) {
		for (const auto& func: program.functions) {
			for (const auto& op: func->code->opcodes) {
				if (op->opcode_name.strView() == "ret_tailcall") {
					if (func->arg_size != func->next_arg_size)
						log.log(makeBox<vm::validator::CallerArgSizeMismatch>(*op->position));

					variant_match(op->args[0].arg) {
						variant_case(vm::opargs::FunctionName, function_name_arg) {
							auto maybe_called_func
								= program.name_to_func.atMaybe(function_name_arg.function_name);
							if_opt_some(maybe_called_func, called_func) {
								if (called_func->arg_size != called_func->next_arg_size) {
									log.log(
										makeBox<vm::validator::CalledArgSizeMismatch>(*op->position)
									);
								}
								if (func->arg_size != called_func->arg_size) {
									log.log(makeBox<vm::validator::CallerCalledArgSizeMismatch>(
										*op->position
									));
								}
								if (func->local_size != called_func->local_size) {
									log.log(makeBox<vm::validator::CallerCalledStackSizeMismatch>(
										*op->position
									));
								}
								if (func->ret_size != called_func->ret_size) {
									log.log(makeBox<vm::validator::CallerCalledRetSizeMismatch>(
										*op->position
									));
								}
							}
						}
						variant_default {
							CORE_ASSERT(
								false, "ret_tailcall should have a function name argument!"
							);
						}
					}
				}
			}
		}
	}

	void Validator::validateDuplicateFunctionDeclarations(
		const parser::ParsedProgram& program, dia::Logger& log
	) {
		return;
	}
}
