#pragma once

#include "dvm_value.hpp"
#include "operations/dvm_operation.hpp"

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

	class CtvLowering;

	class FunctionLoweringContext {
	public:
		friend class InstructionLowerer;
		friend DVMOperation lirInstrToDVMOperation(FunctionLoweringContext&, const lir::Instruction&);

		// Shared compile-time-value lowering (see ctv_lowering.hpp). Befriending the whole struct
		// gives its routines access to the instruction-emission internals (temps, instruction
		// buffer, program context).
		friend class CtvLowering;

		FunctionLoweringContext(
			ProgramLoweringContext&                     program_context,
			base::StrID                                 name,
			CRef<tsl::TypeLayout>                       return_type,
			const std::vector<CRef<tsl::TypeLayout>>&   parameter_types,
			base::Optional<debug_info::FunctionBuilder> fun_di_builder_opt
		);

		/// Constructs a parameterless, void-returning context. Used to synthesize small helper
		/// functions (e.g. global constructors) that have no LIR signature to lower.
		FunctionLoweringContext(ProgramLoweringContext& program_context, base::StrID name);

		FunctionLoweringContext(const FunctionLoweringContext&)            = delete;
		FunctionLoweringContext(FunctionLoweringContext&&)                 = delete;
		FunctionLoweringContext& operator=(const FunctionLoweringContext&) = delete;
		FunctionLoweringContext& operator=(FunctionLoweringContext&&)      = delete;

		void beginBlock(lir::BlockRef block);

		void pushTerminator(const lir::Instruction& lir_terminator);
		void pushInstruction(const lir::Instruction& lir_instruction);


		/**
		 * @brief Translates a LIRPlace to a DVMPlace. In case of direct values returns a place
		 * representing a local/global variable, for references and projection chains (like
		 * a.field[3].*) returns a pointer to final calculated place.
		 */
		DVMPlace resolveLirPlace(const lir::LIRPlace& place);

		/**
		 * @brief Translates a LIRValue to a DVMValue. Performs all needed operations to retrieve
		 * the value.
		 */
		DVMValue lowerLirValue(const lir::LIRValue& lir_value);

		/**
		 * @brief Registers LIR function parameter as a DVM function parameter.
		 * @param lir_func_param LIR local representing a function parameter.
		 * In reality this just means we can use this "already present" local.
		 */
		void registerFunctionParameter(lir::LIRLocalRef lir_func_param);

		/**
		 * @brief Register a LIR local as a DVM local.
		 *
		 * @param lir_local
		 */
		void registerFunctionLocal(lir::LIRLocalRef lir_local);

		DVMPlace getFunctionReturnValueLocal();

		vm::code::Function finish() &&;


	private:
		/**
		 * @brief Creates a mapping between a LIR local and DVM local.
		 */
		const DVMPlace& createLirLocalToDVMMapping(lir::LIRLocalRef local);

		base::StrID getBlockLabel(lir::BlockRef block);

		[[nodiscard]] const DVMPlace& getLirLocal(lir::LIRLocalRef local) const;

		const DVMPlace& getOrInsertLirLocal(lir::LIRLocalRef local);

		/**
		 * @brief Load to temporary local variable from a given pointer type place.
		 * Return the place representing the temporary local variable.
		 */
		DVMPlace loadFromPlace(const DVMPlace& place, const vm::code::TypeOfData& pointee_type);

		/**
		 * @brief Makes sure a given @p value is a place and places it in a temporary if needed
		 * (e.g. the value is an Immediate). If the given value is already a place, it does nothing
		 * and just returns the inner place.
		 */
		DVMPlace forceToPlace(const DVMValue& value, base::Optional<std::string_view> name_hint = {});

		/**
		 * @brief Stores a given @p src_value in @p maybe_dest_place, if the destination was given.
		 * If @p maybe_dest_place is an empty optional it does nothing.
		 * Depending on the place type, performs a `mov_X_X` or a `store_X_X`. Loads immediates to
		 * temporaries if needed.
		 */
		void maybeStoreResult(
			const base::Optional<DVMPlace>& maybe_dest_place, const DVMValue& src_value
		);

		/**
		 * @brief Pushes the inits for the given lifetime flags.
		 *
		 * @param scope_flags
		 */
		void pushInitsForInstr(const std::vector<lir::ScopeFlag>& scope_flags);

		/**
		 * @brief Pushes the deinits for the given lifetime flags.
		 * Works together with `pushed_deinits_for_instr` to ensure deinits are only pushed once per
		 * instruction.
		 *
		 * The typical place for deinits is after the instruction, but in some cases (e.g.
		 * terminators) we may want to push the deinits before the instruction.
		 * @param scope_flags
		 * @param deinits_pushed The boolean flag that used to make sure deinits are only pushed once.
		 */
		void pushDeinitsForInstr(
			const std::vector<lir::ScopeFlag>& scope_flags, bool& deinits_pushed
		);


		void pushInstruction(const vm::code::Instruction& instruction);

		void pushInstruction(const vm::code::builders::InstructionBuilder& instruction);

		/// Push single init instruction
		void pushInit(lir::LIRLocalRef lir_local);

		/// Push single deinit instruction. Local is not needed, but we may want to keep that
		/// information for the future.
		void pushDeinit(lir::LIRLocalRef);

		/**
		 * @brief Removes all existing temporaries added by pushTempLocal, e.g. temps created when
		 * lowering LIRPlace, temps created for comparison operations, etc.
		 */
		void cleanUpRegisteredTemps();

		/**
		 * @brief Pushes a temporary local and based on the @p tracked parameter saves it in the
		 * `current_temp_count`. This temporary local will be automatically deinitialized after
		 * `pushInstruction` is executed.
		 *
		 * @p tracked Used in special cases when we don't want the temporaries to be automatically
		 * deinitialized, e.g. when pushing temporaries to pass as arguments to a call opcode.
		 * These temporaries have to be deinitialized manually.
		 */
		DVMPlace pushTempLocal(
			const vm::code::TypeOfData&      type,
			base::Optional<std::string_view> name_hint = {},
			bool                             tracked   = true
		);

		[[nodiscard]] usize instructionsCount() const;

		ProgramLoweringContext& program_context;

		base::Map<lir::LIRLocalRef, DVMPlace> lir_local_to_dvm;
		base::Map<lir::BlockRef, base::StrID> block_to_label;

		base::Optional<vm::code::TypeOfData> function_return_type;
		std::vector<vm::code::TypeOfData>    function_parameter_types;
		base::StrID                          function_name;
		std::vector<vm::code::Instruction>   function_body;

		usize next_temp_id = 0;

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
