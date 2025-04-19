#include "validator.hpp"

#include "errors.hpp"

#include <diagnostic/logger.hpp>

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/parser/elements.hpp>
#include <vm/loader/validator/detail/stack_state.hpp>

namespace vm::loader::validator {
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
			const Program& program;
			LoaderLogger   log;

			void validateMainExistence();
		};

		std::expected<void, LoaderLogger> Validator::validateProgram() {
			validateMainExistence();

			if (!log.good()) return std::unexpected(std::move(log));

			return {};
		}

		void Validator::validateMainExistence() {
			if (!program.funcMap().contains(base::StrID("main"))) log.logSimple(NO_MAIN_ERR.data());
		}
	}

	std::expected<void, LoaderLogger> verify(const Program& program) {
		return Validator(program).validateProgram();
	}

}
