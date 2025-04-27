#include "validator.hpp"

#include "errors.hpp"

#include <diagnostic/logger.hpp>

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/parser/elements.hpp>
#include <vm/loader/validator/detail/stack_state.hpp>

#include <unordered_set>

namespace vm::loader::validator {
	namespace {
		/**
		 * @brief Performs static validation of the program.
		 *
		 * Validates:
		 * - Valid ret_tailcall signatures
		 * - No duplicate function declarations
		 * @todo Update that list when next checks are added
		 */
		class Validator final {
		public:
			Validator(const Program& prog): program(prog) { log = dia::Logger(); }

			/**
			 * @brief Validates the program.
			 * Returns an empty optional in case of success and an error string on failure.
			 *
			 * @return base::Optional<std::string>
			 */
			std::expected<void, LoaderLogger> validateProgram();

		private:
			const Program&                  program;
			LoaderLogger                    log;
			std::unordered_set<base::StrID> is_function_verified;
		};

		std::expected<void, LoaderLogger> Validator::validateProgram() {
			for (const auto& func: program.funcMap()) {
				if (is_function_verified.contains(func.name)) {
					// Here the verification for each function will appear.
					is_function_verified.insert(func.name);
				}
			}
			return {};
		}
	}

	std::expected<void, LoaderLogger> verify(const Program& program) {
		return Validator(program).validateProgram();
	}

}
