#include "validator.hpp"
#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <expected>
#include <vm/program/opcode_args.hpp>
#include <diagnostic/logger.hpp>
#include "base/optional.hpp"
#include "errors.hpp"
#include "vm/program/instructions.hpp"
#include "vm/program/program.hpp"
#include <vm/preprocessor/validator/detail/stack_state.hpp>
#include <vm/preprocessor/parser/elements.hpp>

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
			Validator(const program::Program& prog, base::Optional<const PosMap&> pos_map):
				  program(prog),
				  pos_map(pos_map) {
				log = dia::Logger();
			}

			/**
			 * @brief Validates the program.
			 * Returns an empty optional in case of success and an error string on failure.
			 *
			 * @return base::Optional<std::string>
			 */
			std::expected<void, dia::Logger> validateProgram();

		private:
			const program::Program&       program;
			base::Optional<const PosMap&> pos_map;
			dia::Logger                   log;
			bool                          good = false;

			bool validateMainExistence();
			bool validateTailcallSignatures();
		};

		std::expected<void, dia::Logger> Validator::validateProgram() {
			ADD_CHECK(validateMainExistence);
			ADD_CHECK(validateTailcallSignatures);

			if (!good || log.bad()) return std::unexpected(std::move(log));

			return {};
		}

		bool Validator::validateMainExistence() {
			return program.functions.contains(base::StrID("main"));
		}

		bool Validator::validateTailcallSignatures() {
			for (const auto& func: program.functions | std::views::values) {
				for (const auto& op: func.body) {
					variant_match(op) {
						variant_case(program::instructions::Op_ret_tailcall_func, op_tailcall) {
							auto maybe_called_func
								= program.functions.atMaybe(op_tailcall.arg0.function_name);
							if_opt_some(maybe_called_func, called_func) {
								match_optional(pos_map) {
									opt_none return false;

									opt_some(map) {
										if (func.arg_size != called_func.arg_size) {
											log.log(
												makeBox<vm::validator::CallerCalledArgSizeMismatch>(
													map.at(&op)
												)
											);
										}
										if (func.local_stack_size != called_func.local_stack_size) {
											log.log(makeBox<
													vm::validator::CallerCalledStackSizeMismatch>(
												map.at(&op)
											));
										}
										if (func.ret_size != called_func.ret_size) {
											log.log(
												makeBox<vm::validator::CallerCalledRetSizeMismatch>(
													map.at(&op)
												)
											);
										}
									}
								}
							}
						}
					}
				}
			}
			return true;
		}
	}

	std::expected<void, dia::Logger>
		verify(const program::Program& program, base::Optional<const PosMap&> pos_map) {
		return Validator(program, pos_map).validateProgram();
	}

}
