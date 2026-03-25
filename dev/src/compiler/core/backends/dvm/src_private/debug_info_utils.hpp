#pragma once

#include <debug_info/debug_info.hpp>
#include <frontend/pst_parser/stable_position.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief Maps the compiler stable position (pst::StablePosition) into a
	 * debug_info::SourcePosition with a PstHashPosition. Both contain the same information, just
	 * wrapped differently, so this is a straightforward mapping.
	 *
	 * It's in the header because it's a trivial mapping and can potentially be a no-op, so it makes
	 * sense to be inline.
	 */
	inline debug_info::SourcePosition mapDIPosition(const pst::StablePosition stable_pos) {
		return { .line_col_position = debug_info::PstHashPostion{
					 .postion_scope_begin = stable_pos.begin_scope_node,
					 .postion_scope_end   = stable_pos.end_scope_node,
				 } };
	}
}
