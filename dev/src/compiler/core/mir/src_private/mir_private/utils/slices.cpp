#include "slices.hpp"

#include <helios/symbols/lang_primitives.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

namespace compiler::mir {
	void sliceBoundsCheck(
		BoundsCheckBuilderContext                      context,
		const helios::SliceTypeData&                   slice_data,
		const MIRValue&                                index,
		const MIRValue&                                slice,  // struct { ptr, length }
		const base::Optional<dia_int::StablePosition>& pos
	) {
		auto& function        = context.function;
		auto& condition_block = context.condition_block;
		auto& fail_block      = context.fail_block;
		auto& ok_block        = context.ok_block;
		auto  scope           = context.scope;

		base::Optional<MIRValue> slice_len;
		variant_match(slice.getVariant()) {
			variant_case(MIRPlace, place) {
				slice_len.emplace(place.withField(function.getContext(), slice_data.len));
			}
			variant_default { CORE_PANIC("Slice base must be a MIRPlace"); }
		}

		auto panic_sym
			= context.function.getContext()
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
		// index < len alone lets negative indices through (signed comparison).
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
			{ index, slice_len.value() },
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
