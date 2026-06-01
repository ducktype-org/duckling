#pragma once

#include "mir_private/mir_builders.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::mir {
	struct ExprBuilderContext final {
		BlockBuilderRef  condition_block;
		BlockBuilderRef  fail_block;
		BlockBuilderRef  ok_block;
		FunctionBuilder& function;
		ScopeRef         scope;
	};

	void sliceBoundsCheck(
		ExprBuilderContext                             context,
		const helios::SliceTypeData&                   slice_data,
		const MIRValue&                                index,
		const MIRValue&                                slice,  // struct { ptr, length }
		const base::Optional<dia_int::StablePosition>& pos
	);
}
