// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "type.hpp"

#include <utility>

vm::fast::Type vm::fast::Type::declareType(base::StrID name, TypeID id, TypeSize size) {
	return Type{ .name = name, .id = id, .size = size, .kind = base::Monostate{} };
}

void vm::fast::Type::definePrimitive(Bytes size) {
	kind = vm::fast::kind::Primitive{ .size = size };
}

void vm::fast::Type::definePointer(TypeCRef inner) {
	kind = vm::fast::kind::Pointer{ .pointed_type = inner };
}

void vm::fast::Type::defineFixedSizeTable(TypeCRef inner, u64 table_size) {
	kind = vm::fast::kind::FixedSizeTable{ .element_type = inner, .element_count = table_size };
}

void vm::fast::Type::defineDynamicTable(TypeCRef inner) {
	kind = vm::fast::kind::DynamicTable{ .element_type = inner };
}

void vm::fast::Type::defineData(
	const std::vector<std::pair<base::StrID, TypeCRef>>& fields_definitions,
	base::Optional<kind::InheritanceMetadata>            inheritance_metadata
) {
	kind = vm::fast::kind::Data{ .fields_definitions   = fields_definitions,
		                         .inheritance_metadata = inheritance_metadata };
}

void vm::fast::Type::defineVariant(
	Bytes type_tag_size, const std::vector<TypeCRef>& variants_definitions
) {
	kind = vm::fast::kind::VariantData{ .type_tag_size        = type_tag_size,
		                                .variant_alternatives = variants_definitions };
}

void vm::fast::Type::defineFunction(std::vector<TypeCRef> parameters, std::vector<TypeCRef> result) {
	kind = vm::fast::kind::Function{ .parameters   = std::move(parameters),
		                             .result_types = std::move(result) };
}

void vm::fast::Type::defineOpaque(Bytes size) { kind = vm::fast::kind::Opaque{ .size = size }; }

[[nodiscard]]
vm::fast::TypeID vm::fast::Type::getID() const {
	return id;
}

[[nodiscard]]
base::StrID vm::fast::Type::getName() const {
	return name;
}

[[nodiscard]]
vm::TypeSize vm::fast::Type::getSize() const {
	CORE_ASSERT(size != TypeSize(-1), "getSize called on type without size (e.g. DynamicTable)");
	return size;
}
