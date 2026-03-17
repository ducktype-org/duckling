#pragma once

#include <debug_info/debug_info.hpp>
#include <frontend/pst_parser/stable_position.hpp>

namespace compiler::backend_vm {
	inline debug_info::SourcePosition translateDIPosition(const pst::StablePosition stable_pos) {
		return { .line_col_position = debug_info::PstHashPostion{
					 .postion_scope_begin = stable_pos.begin_scope_node,
					 .postion_scope_end   = stable_pos.end_scope_node,
				 } };
	}
}
