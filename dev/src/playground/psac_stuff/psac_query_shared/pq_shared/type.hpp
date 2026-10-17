#pragma once

#include "ids.hpp"

#include <frontend/pst_parser/pst_visitor.hpp>

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vector>

namespace pg::tp {

	struct Incomplete {};

	struct Namespace {};

	struct Structure {
		// Fields are stored as symbols in the namespace of the structure
		sym::SymbolID              structure_symbol_id;
		std::vector<sym::SymbolID> field_symbols;
		/// Namespace-qualified name — a stable identity for per-class dependency
		/// tracking in the query-based backend.
		base::StrID qualified_name;
	};

	struct Integral {
		Bytes size;
	};

	struct Boolean {};

	// Distinct from Integral like in the real compiler (tsh::CharAbstractType):
	// chars compare but don't participate in arithmetic. Lowered as i8.
	struct Character {};

	struct Void {};

	struct Pointer {
		TypeID points_to;
	};

	/// C-style array type `T[N]`. Fixed-size arrays lower to llvm [N x T]; a size of
	/// 0 marks a VLA (runtime-sized, locals only), which has no static LLVM type —
	/// its alloca is sized at the declaration site.
	struct Array {
		TypeID element;
		u64    size;

		[[nodiscard]]
		bool isVla() const {
			return size == 0;
		}
	};

	struct Function {
		std::vector<TypeID> param_types;
		TypeID              return_type;
	};

	using TypeKind = std::variant<
		Integral,
		Boolean,
		Character,
		Incomplete,
		Structure,
		Pointer,
		Array,
		Function,
		Void,
		Namespace>;

	struct TypeInfo {
		base::StrID     name;
		TypeKind        kind;
		TypeID          id;
		ns::NamespaceID namespace_id;
	};
}
