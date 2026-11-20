// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <debug_info/debug_info.hpp>

#include <diagnostic/stable_position.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief Maps the compiler stable position (dia::StablePosition) into a
	 * debug_info::SourcePosition with a PstHashPosition. Both contain the same information, just
	 * wrapped differently, so this is a straightforward mapping.
	 *
	 * It's in the header because it's a trivial mapping and can potentially be a no-op, so it makes
	 * sense to be inline.
	 */
	inline debug_info::SourcePosition mapDIPosition(const dia::StablePosition stable_pos) {
		return { .line_col_position = debug_info::PstHashPostion{
					 .postion_scope_begin = stable_pos.begin_node,
					 .postion_scope_end   = stable_pos.end_node,
				 } };
	}
}
