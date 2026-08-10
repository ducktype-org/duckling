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
		 * @brief An ownership transfer this result carries, set by a `move`.
		 *
		 * There are three consumers and each of them resolves it differently:
		 * - `storeResultInGivenPlace()` writes the value into the target place, which becomes its
		 *   owner. `var b = move a` gives `b := Assign a [Move a, Construct b]`.
		 * - `takeOwnership()` is for a consumer that passes the value on as an operand and hands it
		 *   to somebody else, i.e. a call argument. `f(move a)` gives `Call f, a [Move a]`.
		 * - `getResult()` is for a consumer that only reads the value, i.e. `move a` used as a
		 *   statement. Nobody takes the value over, so it is stored into a temporary which gets
		 * 	 destructed at the end of its scope.
		 */
		struct PendingMove final {
			/// The place the value is read from.
			MIRValue source;
			/// The local the `Move` flag moves.
			MIRLocalRef source_local;
		};

		base::Optional<PendingMove> pending_move;

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
		 *
		 * @warning This is the read-only path. A temporary it creates owns the value and is
		 * destructed at the end of its scope. A consumer that hands the value over to somebody else
		 * has to call `takeOwnership()` first, otherwise the value ends up with two owners.
		 */
		[[nodiscard]]
		MIRValue getResult(FunctionBuilder& function);

		/**
		 * @brief Takes over the ownership of the value, if this result carries one.
		 *
		 * For a consumer that passes the result on as an operand and makes somebody else its owner,
		 * i.e. a call argument. Returns the flags its instruction has to carry, so that the source
		 * of the move is marked as moved-out on the instruction that reads it. Call it before
		 * `getResult()`.
		 *
		 * @return The flags to append to the consuming instruction, empty if there is nothing to
		 * take over.
		 */
		[[nodiscard]]
		std::vector<OperationFlag> takeOwnership();

		/**
		 * @brief If the result of the expression is a value, it creates an
		 * instruction that will assign the result to it. Otherwise, it makes the last instruction
		 * of the expression save its result directly to the target.
		 * @note May use InstructionHole stored in structure, should only be called once.
		 *
		 * The target becomes the owner of the value, so this also resolves a pending move.
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
