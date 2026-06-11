#pragma once

#include "ids.hpp"

#include <base/extend_cpp/strongly_typed_id.hpp>

#include <string_id/string_id.hpp>

namespace pg::sym {
	struct FunctionSymbol {
		ns::NamespaceID            primary_namespace;
		std::vector<sym::SymbolID> argument_symbols;
		base::StrID link_name;  ///< Namespace-qualified name used for the LLVM function symbol.
	};

	struct VariableSymbol {};

	/// Compile-time constant. The value is stored as raw 64-bit data; the symbol's
	/// type_id says how to interpret it (i64 or bool for now).
	struct ConstantSymbol {
		u64 bits;
	};

	struct TypeSymbol {
		tp::TypeID type_id;
	};

	struct NamespaceSymbol {
		ns::NamespaceID namespace_id;
	};

	struct FieldSymbol {
		u32 index;
	};

	using SymbolKind = std::variant<
		FunctionSymbol,
		VariableSymbol,
		ConstantSymbol,
		TypeSymbol,
		NamespaceSymbol,
		FieldSymbol>;

	struct Symbol {
		base::StrID name;
		SymbolID    id;
		tp::TypeID  type_id;
		SymbolKind  kind;
	};

}
