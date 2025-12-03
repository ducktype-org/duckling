#pragma once

#include "function_lowering_context.hpp"
#include "lir/lir_structure/lir_structure.hpp"

#include <typesystem/lower/type_layout.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_program.hpp>

#include <expected>

namespace compiler::backend_vm::internal {


	class ProgramLoweringContext {
	public:
		ProgramLoweringContext() {
			collection = vm::code::ValidProgram::withBuiltins().produceValidCodeCollection();
		}

		FunctionLoweringContext getFuncLoweringCtx(CRef<lir::Function> lir_function) {
			return { *this, lir_function };
		}

		/* This might not be necessary
		**
		 * @brief Forwards declaration of a function into the program being built.
		 * This just allows other functions to correctly prepare their stacks when calling this
		function.
		 *
		void forwardDeclareFunction(base::StrID name, const vm::code::FuncSignature& signature) {
		    throw base::NotYetImplemented("forwardDeclareFunction");
		}
		*/

		const DVMGlobal& getDVMGlobal(const lir::LIRGlobal& lir_global);


		/**
		 * @brief Lowers a LIR type layout into VM bytecode type representation.
		 * It inserts the type into the valid program if needed.
		 */
		vm::code::TypeOfData lowerTslType(CRef<tsl::TypeLayout> layout);

		/**
		 * @brief Produces the final bytecode program.
		 * @note It does not consume internal state and can be called multiple times.
		 */
		std::expected<vm::code::CodeCollection, std::string> validateAndProduceProgram() {
			try {
				auto valid = vm::code::ValidProgram::empty();
				valid.tryInsertCode(collection);
				return valid.produceValidCodeCollection();
			} catch (vm::code::ValidationError& e) { return std::unexpected(e.what()); }
		}

	private:
		// The collection being built.
		// Using ValidProgram here would be inefficient due to the need for frequent code verifications.
		vm::code::CodeCollection collection;

		base::Map<lir::LIRGlobal, DVMGlobal> lir_global_to_dvm;
	};
}
