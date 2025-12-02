#pragma once

#include <typesystem/lower/type_layout.hpp>

#include "vm/bytecode/bytecode.hpp"
#include "vm/bytecode/validator/errors.hpp"
#include <vm/bytecode/validator/valid_program.hpp>
#include "function_lowering_context.hpp"

#include <expected>

namespace compiler::backend_vm::internal {
	class ProgramLoweringContext {
	public:
		ProgramLoweringContext() {
			collection = vm::code::ValidProgram::withBuiltins().produceValidCodeCollection();
        };

		FunctionLoweringContext getFuncLoweringCtx(CRef<lir::Function> lir_function) {
            return {*this, lir_function};
        }



		/**
		 * @brief Lowers a LIR type layout into VM bytecode type representation.
		 * It inserts the type into the valid program if needed.
		 */
		vm::code::TypeOfData lowerLIRType(CRef<tsl::TypeLayout> layout);

		/**
		 * @brief Produces the final bytecode program.
		 * @note It does not consume internal state and can be called multiple times.
		 */
		std::expected<vm::code::CodeCollection, std::string> validateAndProduceProgram() {
			try {
				auto valid = vm::code::ValidProgram::empty();
				valid.tryInsertCode(collection);
                return valid.produceValidCodeCollection();
            } catch (vm::code::ValidationError& e) {
                return std::unexpected(e.what());
            }
		}

	private:
		vm::code::CodeCollection collection;
	};
}
