// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "bounds_check.hpp"

#include <helios/symbols/lang_primitives.hpp>

#include <base/except/exceptions.hpp>

namespace compiler::mir {
	void boundsCheck(
		BoundsCheckBuilderContext                  context,
		const MIRValue&                            index,
		const MIRValue&                            length,
		const base::Optional<dia::StablePosition>& pos
	) {
		auto& function        = context.function;
		auto& condition_block = context.condition_block;
		auto& fail_block      = context.fail_block;
		auto& ok_block        = context.ok_block;
		auto  scope           = context.scope;

		auto panic_sym
			= function.getContext()
		          .query<helios::QueryLanguagePrimitiveSymID>({ helios::LanguagePrimitive::Panic })
		          ->valueOrThrow();
		fail_block->addInstruction(Instruction{
			Operation::Call,
			{},
			{ MIRFunctionLiteral{ panic_sym } },
			{},
			scope,
			{},
			{ pos },
		});
		// Maybe we should end the Function here.
		fail_block->setTerminator({ Operation::Jump, {}, { ok_block->getID() }, {}, scope });

		// The index is coerced to a signed integer, so both bounds must be verified:
		// index < length alone lets negative indices through (signed comparison).
		//
		// Note: instructions in a block are assembled in reverse execution order,
		// so the BooleanAnd is added first and executes last.
		auto below_len_tmp    = function.addConditionTmp(scope);
		auto non_negative_tmp = function.addConditionTmp(scope);
		auto condition_tmp    = function.addConditionTmp(scope);

		condition_block->addInstruction(Instruction(
			Operation::BooleanAnd,
			MIRPlace(condition_tmp),
			{ MIRValue(below_len_tmp), MIRValue(non_negative_tmp) },
			{ flagConstruct(condition_tmp) },
			scope,
			{},
			{}
		));

		auto zero = MIRValue{ MIRConstant{
			ctv::CompileTimeValue{ ctv::NumericValue{ static_cast<i64>(0) } } } };

		condition_block->addInstruction(Instruction(
			Operation::IntegerGteq,
			MIRPlace(non_negative_tmp),
			{ index, zero },
			{ flagConstruct(non_negative_tmp) },
			scope,
			{},
			{}
		));

		condition_block->addInstruction(Instruction(
			Operation::IntegerLt,
			MIRPlace(below_len_tmp),
			{ index, length },
			{ flagConstruct(below_len_tmp) },
			scope,
			{},
			{}
		));

		condition_block->setTerminator(Instruction(
			Operation::Branch,
			{},
			{ MIRValue(condition_tmp), ok_block->getID(), fail_block->getID() },
			{},
			scope,
			{},
			{}
		));
	}
}
