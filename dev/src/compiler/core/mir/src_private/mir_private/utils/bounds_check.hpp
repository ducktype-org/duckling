#pragma once

#include <mir/mir_structure/mir_structure.hpp>
#include <mir_private/mir_builders.hpp>

#include <base/collections/optional.hpp>

namespace compiler::mir {
	struct BoundsCheckBuilderContext final {
		BlockBuilderRef  condition_block;
		BlockBuilderRef  fail_block;
		BlockBuilderRef  ok_block;
		FunctionBuilder& function;
		ScopeRef         scope;
	};

	/**
	 * @brief Emits a bounds check `0 <= index < length`. Shared by slices, dynamic arrays
	 * and static arrays.
	 *
	 * @param index  The (signed) index value being checked.
	 * @param length The length of the indexed array.
	 * @param pos    Source position of the indexing expression.
	 */
	void boundsCheck(
		BoundsCheckBuilderContext                      context,
		const MIRValue&                                index,
		const MIRValue&                                length,
		const base::Optional<dia_int::StablePosition>& pos
	);
}
