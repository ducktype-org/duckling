#include "validator.hpp"
#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <vm/program/opcode_args.hpp>
#include <diagnostic/logger.hpp>
#include "errors.hpp"
#include "vm/program/instructions.hpp"
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
			Validator(const program::Program& prog): program(prog) { log = dia::Logger(); }

			/**
			 * @brief Validates the program.
			 * Returns an empty optional in case of success and an error string on failure.
			 *
			 * @return base::Optional<std::string>
			 */
			base::Optional<std::string> validateProgram();

		private:
			const program::Program& program;
			dia::Logger             log;

			void preprocessProgram();
			void validateMainExistance();
			void validateTailcallSignatures();
			void validateDuplicateFunctionDeclarations();
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
				if (func.name.strView() == "main") {
					main_found = true;
					break;
				}
			}

			if (!main_found) {
				CORE_PANIC("No main");
				// log.log(makeBox<vm::validator::NoMainError>(*program.files_src_pos[0]));
			}
		}

		void Validator::validateTailcallSignatures() {
			for (const auto& func: program.functions) {
				for (const auto& op: func.body) {
					variant_match(op) {
						variant_case(program::instructions::Op_ret_tailcall_func, name) {
							auto maybe_called_func
								= program.name_to_func.atMaybe(function_name_arg.function_name);
							if_opt_some(maybe_called_func, called_func) {
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
					}
				}
			}
		}

		void Validator::validateDuplicateFunctionDeclarations() { return; }
	}

	base::Optional<std::string> verify(const parser::ParsedProgram& program) {
		return Validator(program).validateProgram();
	}

}
