#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>

#include <vector> // Needed for std::hash

namespace pg {
	namespace tp {
		STRONG_TYPEDEF_ID(TypeID);
	}

	namespace sym {
		STRONG_TYPEDEF_ID(SymbolID);
	}

	namespace ns {
		STRONG_TYPEDEF_ID(NamespaceID);
	}
}

ID_STD_HASH(pg::tp::TypeID);
ID_STD_HASH(pg::sym::SymbolID);
ID_STD_HASH(pg::ns::NamespaceID);
