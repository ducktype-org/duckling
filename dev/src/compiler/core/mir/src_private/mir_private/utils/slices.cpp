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
		auto condition_tmp = function.addConditionTmp(scope);

		condition_block->addInstruction(Instruction(
			Operation::IntegerLt,
			MIRPlace(condition_tmp),
			{ index, slice_len.value() },
			{ flagConstruct(condition_tmp) },
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
