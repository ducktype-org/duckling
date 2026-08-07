#pragma once

#include "mir_builders.hpp"

#include <helios/hout/elements/expr.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <variant>

namespace compiler::mir {
	namespace hc = helios::code;

	/**
	 * @brief Represents a partial result of expression lowering.
	 *
	 * This consists of a BlockBuilderRef marking the beginning of the lowered
	 * expression and either:
	 *  - a MIRValue holding the result of the expression, OR
	 *  - a Finalizer representing the last instruction that saves the result
	 *    without specifying its target.
	 *
	 * For complete lowering, call the dedicated function that stores the result
	 * in the desired location while (if possible) avoiding the creation of unnecessary temporaries.
	 *
	 * In most cases, use getResult() or storeResultInGivenPlace().
	 */
	struct ExprLowerRes final {
		BlockBuilderRef begin;

		/**
		 * @brief Represents a finalizer instruction that saves the result of an expression.
		 * Stores hole where instruction will be saved, instruction without output and type of
		 * result. This instruction can be performed on provided variable
		 * (storeResultInGivenPlace) or generated temporary (getResult).
		 */
		struct Finalizer final {
			BlockBuilder::InstructionHole hole;
			Instruction                   instr;
			tsh::SymbolType<>             type;
		};

		std::variant<MIRValue, Finalizer> value;

		/**
		 * @brief Whether producing this result hands the ownership of the value over to whoever
		 * consumes it. Set for the result of a move.
		 *
		 * For example:
		 * `foo(goo())` (which in reality is `foo(implicit_move(goo()))`)
		 * lowers to:
		 * ```
		 * tmp = goo()            // Move flag
		 * foo(tmp)
		 * ```
		 *
		 * `tmp` should not be destroyed after the scope ends since it's "in the process of moving"
		 * to `foo()`, thus this bool makes sure to add a `NoDestructor` flag to it.
		 *
		 * When the result is written straight into a target dest instead, no such temporary is
		 * created and this changes nothing.
		 */
		bool hands_over_ownership = false;

		ExprLowerRes(BlockBuilderRef begin, std::variant<MIRValue, Finalizer> value);

		/**
		 * @brief helper function returning type of result. Can be used if MIRValue is not stored.
		 */
		[[nodiscard]]
		tsh::SymbolType<> getResultType();

		/**
		 * @brief Helper function that returns MIRvalue if it is already stored in structure.
		 */
		[[nodiscard]]
		base::Optional<MIRValue> getResultIfStored();

		/**
		 * @brief If result of the expression is a value already returns it,
		 * Otherwise creates temporary, makes last instruction save res there and returns it.
		 * @note may use InstructionHole stored in structure, probably use only once.
		 */
		[[nodiscard]]
		MIRValue getResult(FunctionBuilder& function);

		/**
		 * @brief If the result of the expression is a value, it creates an
		 * instruction that will assign the result to it. Otherwise, it makes the last instruction
		 * of the expression save its result directly to the target.
		 * @note May use InstructionHole stored in structure, should only be called once.
		 */
		void storeResultInGivenPlace(
			const MIRPlace&                   target,
			BlockBuilder::InstructionHole&    hole,
			const std::vector<OperationFlag>& flags,
			ScopeRef                          scope,
			InstructionMetadata               metadata
		);
	};

	/**
	 * @brief Lowers expression.
	 *
	 * @param expr
	 * @param continuation Block that should be executed after this expression.
	 * @param function Function that we are lowering this expression in.
	 * @param expr_scope Lifetime Scope this expression should be in.
	 * @return ExprLowerRes
	 */
	ExprLowerRes lowerExpr(
		const hc::Expr&  expr,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         expr_scope
	);
}
