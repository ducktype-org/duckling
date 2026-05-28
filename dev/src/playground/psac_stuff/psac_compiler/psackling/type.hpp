#pragma once

#include "ids.hpp"

#include <frontend/pst_parser/pst_visitor.hpp>

#include "base/types/bits_and_bytes.hpp"
#include <base/extend_cpp/strongly_typed_id.hpp>

#include <string_id/string_id.hpp>

#include <vector>

namespace pg::tp {

	struct Incomplete {};

	struct Namespace {};

	struct Structure {
		// Fields are stored as symbols in the namespace of the structure
		sym::SymbolID              structure_symbol_id;
		std::vector<sym::SymbolID> field_symbols;
	};

	struct Integral {
		Bytes size;
	};

	struct Boolean {};

	struct Void {};

	struct Pointer {
		TypeID points_to;
	};

	struct Function {
		std::vector<TypeID> param_types;
		TypeID              return_type;
	};

	using TypeKind
		= std::variant<Integral, Boolean, Incomplete, Structure, Pointer, Function, Void, Namespace>;

	struct TypeInfo {
		base::StrID     name;
		TypeKind        kind;
		TypeID          id;
		ns::NamespaceID namespace_id;
	};
}
