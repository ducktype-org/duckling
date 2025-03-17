#include "validator.hpp"
#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <vm/code_data/opcode_args.hpp>
#include <diagnostic/logger.hpp>
#include "base/string_id.hpp"
#include "errors.hpp"
#include <vm/preprocessor/validator/detail/stack_state.hpp>
#include <vm/preprocessor/parser/elements.hpp>
#include <sstream>

namespace vm::validator {
	namespace {
		/**
		 * @brief Performs static validation of the program.
		 *
		 * Validates:
		 * - Main function existence
		 * - Valid ret_tailcall signatures
		 * - No duplicate function declarations
		 * @todo Update that list when next checks are added
		 */
		class Validator {
		public:
			Validator(const parser::ParsedProgram& prog): program(prog) { log = dia::Logger(); }

			/**
			 * @brief Validates the program.
			 * Returns an empty optional in case of success and an error string on failure.
			 *
			 * @return base::Optional<std::string>
			 */
			base::Optional<std::string> validateProgram();

		private:
			const parser::ParsedProgram& program;
			dia::Logger                  log;

			void preprocessProgram();
			void validateMainExistance();
			void validateTailcallSignatures();
			void validateDuplicateFunctionDeclarations();
			void validateJumpStackStructure();
		};

		base::Optional<std::string> Validator::validateProgram() {
			validateMainExistance();
			validateTailcallSignatures();
			validateDuplicateFunctionDeclarations();

			if (log.bad()) {
				std::stringstream stream;
				log.dumpLogAndClear(true, stream);
				return stream.str();
			}
			return {};
		}

		void Validator::validateMainExistance() {
			bool main_found = false;
			for (auto& func: program.functions) {
				if (func->name.value.strView() == "main") {
					main_found = true;
					break;
				}
			}

			if (!main_found)
				log.log(makeBox<vm::validator::NoMainError>(*program.files_src_pos[0]));
		}

		void Validator::validateTailcallSignatures() {
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
										log.log(makeBox<vm::validator::CalledArgSizeMismatch>(
											*op->position
										));
									}
									if (func->arg_size != called_func->arg_size) {
										log.log(makeBox<vm::validator::CallerCalledArgSizeMismatch>(
											*op->position
										));
									}
									if (func->local_size != called_func->local_size) {
										log.log(
											makeBox<vm::validator::CallerCalledStackSizeMismatch>(
												*op->position
											)
										);
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

		void Validator::validateDuplicateFunctionDeclarations() { return; }

		void Validator::validateJumpStackStructure() {
			for (const auto& func: program.functions) {
				int instruction_counter = 0;
				int current_depth = 0;
				using JumpRange = struct {int depth; int l; int r;};
				base::HashMap<base::StrID, JumpRange> jump_ranges;
				for (const auto& op: func->code->opcodes) {
					if (op->opcode_name.strView() == "init_type") {
						current_depth++;
					} else if (op->opcode_name.strView() == "deinit") {
						current_depth--;
					} else if (op->opcode_name.strView() == "label" || op->opcode_name.strView() == "jmpRel_label") { // TODO: change to all jumps
						variant_match(op->args[0].arg) {
							variant_case(opargs::Label, label) {
								base::StrID label_name = label.label_name;
								if (jump_ranges.contains(label_name)) {
									if (jump_ranges[label_name].depth != current_depth) {
										// Stack structure invalid!!!
									}
									jump_ranges[label_name].r = instruction_counter;
								}
								else {
									jump_ranges[label_name] = {.depth=current_depth, .l=instruction_counter, .r=instruction_counter};
								}
							}
							variant_default {
								CORE_ASSERT(false, "expected label name after jump opcode");
							}
						}
					}
					instruction_counter++;
				}
				// TODO: check if minimum in range is equal to stored depth
			}
		}
	}

	base::Optional<std::string> verify(const parser::ParsedProgram& program) {
		return Validator(program).validateProgram();
	}

}
