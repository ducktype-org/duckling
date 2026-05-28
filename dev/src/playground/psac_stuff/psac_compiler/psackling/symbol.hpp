#include "ids.hpp"

#include <base/extend_cpp/strongly_typed_id.hpp>

#include <string_id/string_id.hpp>

namespace pg::sym {
	struct FunctionSymbol {
		ns::NamespaceID            primary_namespace;
		std::vector<sym::SymbolID> argument_symbols;
	};

	struct VariableSymbol {};

	struct TypeSymbol {
		tp::TypeID type_id;
	};

	struct NamespaceSymbol {
		ns::NamespaceID namespace_id;
	};

	struct FieldSymbol {
		u32 index;
	};

	using SymbolKind
		= std::variant<FunctionSymbol, VariableSymbol, TypeSymbol, NamespaceSymbol, FieldSymbol>;

	struct Symbol {
		base::StrID name;
		SymbolID    id;
		tp::TypeID  type_id;
		SymbolKind  kind;
	};

}
