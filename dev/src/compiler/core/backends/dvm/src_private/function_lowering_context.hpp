#pragma once

#include "dvm_operation.hpp"
#include "dvm_value.hpp"
#include "operations/arithmetic_operation_lowering.hpp"

#include <debug_info/debug_info_builder.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/type_layout.hpp>

#include <base/pointers/ref.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/utils/interpret.hpp>

namespace compiler::backend_vm::internal {
	class ProgramLoweringContext;

	class FunctionLoweringContext {
	public:
		friend class MetaOperationLowerer;
		friend class CastOperationLowerer;
		friend class ComparisonOperationLowerer;
		friend class ArithmeticOperationLowerer;
		friend DVMOperation lirInstrToDVMOperation(FunctionLoweringContext&, const lir::Instruction&);

		FunctionLoweringContext(
			ProgramLoweringContext&                     program_context,
			base::StrID                                 name,
			CRef<tsl::TypeLayout>                       return_type,
			const std::vector<CRef<tsl::TypeLayout>>&   parameter_types,
			base::Optional<debug_info::FunctionBuilder> fun_di_builder_opt
		);

		FunctionLoweringContext(const FunctionLoweringContext&)            = delete;
		FunctionLoweringContext(FunctionLoweringContext&&)                 = delete;
		FunctionLoweringContext& operator=(const FunctionLoweringContext&) = delete;
		FunctionLoweringContext& operator=(FunctionLoweringContext&&)      = delete;

		void beginBlock(lir::BlockRef block);

		void pushTerminator(const lir::Instruction& lir_terminator);
		void pushInstruction(const lir::Instruction& lir_instruction);

		[[deprecated(
			"@TODO: #1656 Move this temporary helper when inits/deinits are handled correctly to "
			"pushInstruction's implementation of init"
		)]]
		void pushInit(lir::LIRLocalRef lir_local);

		/**
		 * @brief Registers LIR function parameter as a DVM function parameter.
		 * @param lir_func_param LIR local representing a function parameter.
		 * In reality this just means we can use this "already present" local.
		 */
		void registerFunctionParameter(lir::LIRLocalRef lir_func_param);

		DVMLocal getFunctionReturnValueLocal();

		vm::code::Function finish() &&;

		struct FunctionCallInfo {
			DVMCallable                          call_target;
			base::Optional<vm::code::TypeOfData> return_type;
			std::vector<vm::code::TypeOfData>    param_types;
			bool                                 is_extern_c;

			/**
			 * @brief Created call info for a LIR function.
			 * Translates TSL type layouts to corresponding DVM types.
			 */
			static FunctionCallInfo fromLirFunction(
				const lir::FunctionLiteral& func_literal, ProgramLoweringContext& program_context
			);

			/**
			 * @brief Creates call info for an extern C function.
			 * Translates type names from extern C function signatures to corresponding DVM types.
			 */
			static FunctionCallInfo fromExternCFunction(
				const base::StrID& func_name, ProgramLoweringContext& program_context
			);
		};

	private:
		/**
		 * @brief Translates a LIRPlace to a DVMPlace. In case of direct values returns a place
		 * representing a local/global variable, for references and projection chains (like
		 * a.field[3].*) returns a pointer to final calculated place.
		 */
		DVMPlace resolveLirPlace(const lir::LIRPlace& place);

		/**
		 * @brief Creates a mapping between a LIR local and DVM local.
		 */
		const DVMLocal& createLirLocalToDVMMapping(lir::LIRLocalRef local);

		base::StrID getBlockLabel(lir::BlockRef block);

		const DVMLocal&               insertLirLocal(lir::LIRLocalRef local);
		[[nodiscard]] const DVMLocal& getLirLocal(lir::LIRLocalRef local) const;

		DVMValue lowerLirValue(const lir::LIRValue& lir_value);

		DVMLocal forceToLocal(const DVMValue& value, base::Optional<std::string_view> name_hint = {});

		/**
		 * @brief Stores a given @p src_value in @p dest_place.
		 * Depending on the place type, performs a `mov_X_X` or a `store_X_X`.
		 * Loads immediates to temporaries if needed.
		 */
		void storeResult(const DVMPlace& dest_place, const DVMValue& src_value);

		void pushInstruction(const vm::code::Instruction& instruction);

		void pushInstruction(const vm::code::builders::InstructionBuilder& instruction);

		/**
		 * @brief Removes all existing temporaries added by pushTempLocal, e.g. temps created when
		 * lowering LIRPlace, temps created for comparison operations, etc.
		 */
		void cleanUpRegisteredTemps();

		void handleCall(
			const FunctionCallInfo&     call_info,
			const std::deque<DVMValue>& func_args,
			base::Optional<DVMPlace>    output
		);

		usize next_temp_id = 0;
		/**
		 * @brief Pushes a temporary local and based on the @p tracked parameter saves it in the
		 * `current_temp_count`. This temporary local will be automatically deinitialized after
		 * `pushInstruction` is executed.
		 *
		 * @p tracked Used in special cases when we don't want the temporaries to be automatically
		 * deinitialized, e.g. when pushing temporaries to pass as arguments to a call opcode.
		 * These temporaries have to be deinitialized manually.
		 */
		DVMLocal pushTempLocal(
			const vm::code::TypeOfData&      type,
			base::Optional<std::string_view> name_hint = {},
			bool                             tracked   = true
		);

		[[nodiscard]] usize instructionsCount() const;

		ProgramLoweringContext& program_context;

		base::Map<lir::LIRLocalRef, DVMLocal> lir_local_to_dvm;
		base::Map<lir::BlockRef, base::StrID> block_to_label;

		vm::code::TypeOfData               function_return_type;
		std::vector<vm::code::TypeOfData>  function_parameter_types;
		base::StrID                        function_name;
		std::vector<vm::code::Instruction> function_body;

		/**
		 * @brief Number of temporaries created by the currently lowered instruction.
		 * Gets cleared by `cleanupInstructionTemps()` after each call of `pushInstruction`.
		 */
		usize current_temp_count{ 0 };

		/**
		 * @brief Optional debug info builder for the function.
		 */
		base::Optional<debug_info::FunctionBuilder> fun_di_builder_opt;
	};
}
