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

		void lower(const NoOpOperation&) {}

		void lower(const UnaryOperation& op);
		void lower(const BinaryOperation& op);
		void lower(const MoveOperation& op);
		void lower(ComparisonOperation& op);
		void lower(const CallOperation& op);
		void lower(const AddressOfOperation& op);
		void lower(const CastOperation& op);
		void lower(const MetaOperation& op);

		// Terminators
		void lower(const JumpOperation& op);
		void lower(const BranchOperation& op);
		void lower(const ReturnOperation& op);

	private:
		Ref<FunctionLoweringContext> ctx;
	};
}
