#pragma once

#include <frontend/pst_parser/pst_visitor.hpp>

#include <base/extend_cpp/strongly_typed_id.hpp>

#include <string_id/string_id.hpp>

#include <vector>

namespace pg::tp {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeID);

	struct Structure {
		std::vector<std::pair<base::StrID, base::StrID>> fields;
	};

	struct Integral {
		usize size;
	};

	struct Boolean {};

	struct Pointer {
		TypeID points_to;
	};

	struct TypeInfo {
		base::StrID                                                         name;
		std::variant<std::monostate, Integral, Boolean, Structure, Pointer> type;
		TypeID                                                              id;
	};
}

ID_STD_HASH(pg::tp::TypeID);
