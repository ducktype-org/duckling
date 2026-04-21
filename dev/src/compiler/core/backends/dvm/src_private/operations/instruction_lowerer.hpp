#pragma once

#include "dvm_operation.hpp"

namespace compiler::backend_vm::internal {

	class FunctionLoweringContext;

	/**
	 * @brief A stateful visitor that lowers a DVMOperation into a series of
	 * DVM instructions within a given FunctionLoweringContext.
	 */
	class InstructionLowerer {
	public:
		explicit InstructionLowerer(Ref<FunctionLoweringContext> ctx): ctx(ctx) {}

		// Normal instructions
		void lower(const NoOpOperation& op);
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
