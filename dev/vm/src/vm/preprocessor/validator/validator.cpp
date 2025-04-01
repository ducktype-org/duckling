#include "validator.hpp"

#include "errors.hpp"

#include <diagnostic/logger.hpp>

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/code/code.hpp>
#include <vm/code/instructions.hpp>
#include <vm/code/opcode_args.hpp>
#include <vm/preprocessor/parser/elements.hpp>
#include <vm/preprocessor/preprocessor.hpp>
#include <vm/preprocessor/validator/detail/stack_state.hpp>

#include <expected>

#define ADD_CHECK(name) \
	if (!name()) good = false;

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
			Validator(const Program& prog): program(prog) { log = dia::Logger(); }

			/**
			 * @brief Validates the program.
			 * Returns an empty optional in case of success and an error string on failure.
			 *
			 * @return base::Optional<std::string>
			 */
			std::expected<void, PreprocessorLogger> validateProgram();

		private:
			const Program&     program;
			PreprocessorLogger log;

			void validateMainExistence();
			void validateTailcallSignatures();
		};

		std::expected<void, PreprocessorLogger> Validator::validateProgram() {
			validateMainExistence();
			validateTailcallSignatures();

			if (!log.good()) return std::unexpected(std::move(log));

			return {};
		}

		void Validator::validateMainExistence() {
			if (!program.funcMap().contains(base::StrID("main"))) log.logSimple<NoMainError>();
		}

		void Validator::validateTailcallSignatures() {
			for (const auto& func: program.funcMap()) {
				for (const auto& op: func.body) {
					variant_match(op) {
						variant_case(code::instructions::Op_ret_tailcall_func, op_tailcall) {
							auto maybe_called_func
								= program.funcMap().atMaybe(op_tailcall.arg0.function_name);
							if_opt_some(maybe_called_func, called_func) {
								if (func.arg_size != called_func->arg_size)
									log.log<vm::validator::CallerCalledArgSizeMismatch>(op_tailcall
									);
								if (func.local_stack_size != called_func->local_stack_size)
									log.log<vm::validator::CallerCalledStackSizeMismatch>(
										op_tailcall
									);
								if (func.ret_size != called_func->ret_size)
									log.log<vm::validator::CallerCalledRetSizeMismatch>(op_tailcall
									);
							}
						}
					}
				}
			}
		}
	}

	std::expected<void, PreprocessorLogger> verify(const Program& program) {
		return Validator(program).validateProgram();
	}

}
