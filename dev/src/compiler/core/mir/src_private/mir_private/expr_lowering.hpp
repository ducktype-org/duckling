// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
	 *  - a MovedValue holding a result whose ownership is being handed over, OR
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

		/**
		 * @brief The result of a `move` - a value that is stored in a place, but whose
		 * source local stops owning it.
		 *
		 * The `Move` flag has to sit on an instruction, and which instruction that is depends on
		 * who consumes the result:
		 * - `storeResultInGivenPlace()` writes the value into the target place, which becomes its
		 *   owner. `var b = move a` gives `b := Assign a [Move a, Construct b]`.
		 * - `getResultAndTakeOwnership()` is for a consumer that passes the value on as an operand
		 *   and hands it to somebody else, i.e. a call argument. `f(move a)` gives
		 *   `Call f, a [Move a]`.
		 * - `getResult()` is for a consumer that only reads the value, i.e. `move a` used as a
		 *   statement. Nobody takes the value over, so `owning_assign` puts it in a temporary that
		 *   gets destructed at the end of its scope.
		 */
		struct MovedValue final {
			/// The place the value is read from.
			MIRValue source;
			/// The local the `Move` flag moves.
			MIRLocalRef source_local;
			/// The `Assign` that carries the `Move` flag when no consumer takes the value over.
			Finalizer owning_assign;
		};

		using Storage = std::variant<MIRValue, MovedValue, Finalizer>;
		Storage value;

		ExprLowerRes(BlockBuilderRef begin, Storage value);

		/**
		 * @brief helper function returning type of result. Can be used if MIRValue is not stored.
		 */
		[[nodiscard]]
		tsh::SymbolType<> getResultType();

		/**
		 * @brief Helper function that returns MIRvalue if it is already stored in structure.
		 *
		 * @note A MovedValue counts as not stored, even though it holds a place. Passing the raw
		 * place out would lose the `Move` flag that has to land on some instruction, so a moved
		 * result always has to go through one of the three consumers below.
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
		 * has to use `getResultAndTakeOwnership()` instead, otherwise the value ends up with two
		 * owners. A binary operator or a dereference only reads its operand and does not become
		 * responsible for it, so those stay here.
		 */
		[[nodiscard]]
		MIRValue getResult(FunctionBuilder& function);

		/**
		 * @brief `getResult()` for a consumer that makes somebody else the owner of the value, i.e.
		 * a call argument.
		 *
		 * @p flags Flags the consuming instruction has to carry. New move flags can be appended in
		 * this function.
		 */
		[[nodiscard]]
		MIRValue getResultAndTakeOwnership(
			FunctionBuilder& function, std::vector<OperationFlag>& flags
		);

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
