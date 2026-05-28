#pragma once

#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>  // Stupid PST imports
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>

#include <string_id/string_id.hpp>

namespace pg {
	base::StrID extractTypeName(const pst::ExprElement& expr);
}
