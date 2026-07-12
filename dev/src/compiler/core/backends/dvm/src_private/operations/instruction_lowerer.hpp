#pragma once

#include "dvm_operation.hpp"

namespace compiler::backend_vm::internal {
	class FunctionLoweringContext;

	/**
	 * @brief Visitor that lowers a DVMOperation into a series of
	 * DVM instructions within a given FunctionLoweringContext.
	 *
	 * @note This is a class for all FunctionLoweringContext methods to be usable when lowering the
	 * instructions and friend only one class.
	 */
	class InstructionLowerer {
	public:
		explicit InstructionLowerer(Ref<FunctionLoweringContext> ctx): ctx(ctx) {}

		void lower(const NoOperation&) {}

		void lower(const UnaryOperation& op);
		void lower(const BinaryOperation& op);
		void lower(const MoveOperation& op);

		// This is non-const on purpose. We use a `std::swap` trick in the implementation.
		void lower(ComparisonOperation& op);

		void lower(const CallOperation& op);
		void lower(const BuiltinCallOperation& op);
		void lower(const AddressOfOperation& op);
		void lower(const CastOperation& op);
		void lower(const MetaOperation& op);

		// Terminators
		void lower(const JumpOperation& op);
		void lower(const BranchOperation& op);
		void lower(const ReturnOperation& op);

		/**
		 * @brief Makes sure deinits for the current instruction are only pushed once.
		 */
		bool pushed_deinits_for_instr{ false };

	private:
		Ref<FunctionLoweringContext> ctx;
	};
}
