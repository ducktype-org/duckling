#pragma once

#include "dvm_value.hpp"

#include <lir/lir_structure/lir_structure.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_program.hpp>

namespace compiler::backend_vm::internal {
	class ProgramLoweringContext {
	public:
		ProgramLoweringContext() = default;

		/**
		 * @brief Lowers a LIR function into DVM bytecode function.
		 * @note If the function was already lowered, this is a no-op.
		 */
		const vm::code::Function& lowerAndKeepLirFunction(CRef<lir::Function> lir_function);

		const vm::code::GlobalData& lowerAndKeepLirGlobal(
			const lir::LIRGlobal&               lir_global,
			base::Optional<CRef<lir::Function>> global_ctor,
			base::Optional<CRef<lir::Function>> global_dtor
		);

		/**
		 * @brief Lowers a LIR type layout into VM bytecode type representation.
		 * It caches the result, so inserts the type into the program only if needed.
		 */
		const vm::code::TypeOfData& lowerAndKeepTslType(CRef<tsl::TypeLayout> layout);

		/**
		 * @brief Retrieves the DVM global variable corresponding to the given LIR global.
		 * @note The LIR global must have been previously declared using insertLirGlobal,
		 * panics otherwise.
		 */
		[[nodiscard]] const DVMGlobal& getLirGlobal(CRef<lir::LIRGlobal> lir_global) const;

		/**
		 * @brief Retrieves the extern C function with the given name.
		 * @note The extern C function must have been previously declared using
		 * insertExternCFunction, panics otherwise.
		 */
		[[nodiscard]] const vm::code::ExternalCFunction& getExternCFunction(
			const base::StrID& func_name
		) const;

		/**
		 * @brief Inserts an extern C function into the program context.
		 */
		void insertExternCFunction(const vm::code::ExternalCFunction& extern_func);

		/**
		 * @brief Gets a TypeOfData from a type name (for primitives and opaque types).
		 *
		 * This is used for lowering extern C function signatures that reference
		 * types by name rather than by TSL layout.
		 *
		 * @param type_name The name of the type (e.g., "i64", "opaque_ptr")
		 * @return The corresponding TypeOfData
		 */
		[[nodiscard]] vm::code::TypeOfData getTypeFromName(base::StrID type_name) const;

		/**
		 * @brief Insert raw bytecode into program context.
		 */
		void insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode);

		/**
		 * @brief Produces the final bytecode program.
		 * @note It does not consume internal state and can be called multiple times.
		 */
		std::expected<vm::code::CodeCollection, std::string> validateAndProduceProgram();

	private:
		vm::code::TypeOfData lowerTslTypeInternal(CRef<tsl::TypeLayout> layout);

		// Using ValidProgram here would be inefficient due to the need for frequent code verifications.

		base::Map<CRef<lir::Function>, vm::code::Function> lir_function_to_dvm;

		base::Map<CRef<tsl::TypeLayout>, vm::code::TypeOfData> tsl_type_to_dvm;

		// Extern function name to definition.
		base::Map<base::StrID, vm::code::ExternalCFunction> extern_c_functions;

		// Additional, non-lir functions loaded into a module. Used in CTE.
		std::vector<vm::code::Function> extra_bytecode_functions;

		// Using names as keys to avoid issues with CRef hash/equality.
		base::HashMap<base::StrID, DVMGlobal>            global_name_to_dvm;
		base::HashMap<base::StrID, vm::code::GlobalData> global_name_to_dvm_data;
	};
}
