#include "type.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <algorithm>
#include <cmath>
#include <variant>

using namespace vm::code;

valid_type::ValidType valid_type::ValidType::declareType(base::StrID name, ValidTypeID id) {
	return { name, id };
}

void valid_type::ValidType::definePrimitive(Bytes size) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind = concrete::Primitive{ size };
}

void valid_type::ValidType::definePointer(ValidTypeID inner) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind = concrete::Pointer{ inner };
}

void valid_type::ValidType::defineFixedSizeTable(ValidTypeID inner, usize element_count) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind = concrete::FixedSizeTable{ .inner = inner, .element_count = element_count };
}

void valid_type::ValidType::defineDynamicTable(ValidTypeID inner) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind = concrete::DynamicTable{ .inner = inner };
}

void valid_type::ValidType::defineData(
	const std::vector<std::pair<base::StrID, ValidTypeID>>& fields_definitions
) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	ObjIdNameMap<concrete::Field, concrete::Field::ID> fields;
	for (const auto& field_def: fields_definitions)
		fields.insert(
			concrete::Field{ .offset = TypeSize(Bytes(0), 0),  // Offsets are calculated later
		                     .name   = field_def.first,
		                     .type   = field_def.second },
			field_def.first
		);

	concrete::Structure structure{
		.fields               = std::move(fields),
		.inheritance_metadata = {},
	};

	kind = structure;
}

void valid_type::ValidType::defineClass(
	const std::vector<std::pair<base::StrID, ValidTypeID>>& fields_definitions,
	const bool                                              is_abstract,
	const base::Optional<ValidTypeID>&                      extends,
	const std::vector<ValidTypeID>&                         implements,
	const std::vector<std::pair<base::StrID, ValidTypeID>>& new_virtual_methods,
	const std::vector<std::pair<base::StrID, base::StrID>>& implementations
) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	auto strukt = concrete::Structure();
	for (const auto& field_def: fields_definitions)
		strukt.fields.insert(
			concrete::Field{ .offset = TypeSize(Bytes(0), 0),
		                     .name   = field_def.first,
		                     .type   = field_def.second },
			field_def.first
		);

	base::HashMap<base::StrID, ValidTypeID> virtual_methods;
	for (const auto& method: new_virtual_methods) virtual_methods.put(method.first, method.second);

	base::HashMap<base::StrID, base::StrID> vtable;
	for (const auto& impl: implementations) vtable.put(impl.first, impl.second);

	// Forward the data to be used in inheritance metadata construction during finalization.
	strukt.inheritance_metadata = concrete::InheritanceMetadata {
		.super_types       = {},
		.implements        = implements | std::ranges::to<std::unordered_set>(),
		.available_methods = virtual_methods, .vtable = vtable,
		.kind = concrete::InheritanceMetadata::ClassKind{
			.extends     = extends,
			.is_abstract = is_abstract,
		},
	};

	kind = strukt;
}

void valid_type::ValidType::defineInterface(
	const std::vector<ValidTypeID>&                         implements,
	const std::vector<std::pair<base::StrID, ValidTypeID>>& new_virtual_methods,
	const std::vector<std::pair<base::StrID, base::StrID>>& implementations
) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	base::HashMap<base::StrID, ValidTypeID> virtual_methods;
	for (const auto& method: new_virtual_methods) virtual_methods.put(method.first, method.second);

	base::HashMap<base::StrID, base::StrID> vtable;
	for (const auto& impl: implementations) vtable.put(impl.first, impl.second);

	kind = concrete::Structure{
		.fields = {},          // Interfaces do not have fields
		.inheritance_metadata  // Forward the data to be used in inheritance metadata construction
		                       // during finalization.
		= concrete::InheritanceMetadata{ .super_types = {},
		                                 .implements
		                                 = implements | std::ranges::to<std::unordered_set>(),
		                                 .available_methods = virtual_methods,
		                                 .vtable            = vtable,
		                                 .kind = concrete::InheritanceMetadata::InterfaceKind() },
	};
}

void valid_type::ValidType::defineVariant(const std::vector<ValidTypeID>& variant_types) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	CORE_ASSERT(!variant_types.empty(), "Cannot define variant with no alternatives");
	state = State::Defined;

	// log_256(x) = log_2(x) / log_2(256) = log_2(x) / 8.0
	const auto needed_bytes = ceil(log2(static_cast<double>(variant_types.size())) / 8.0);

	// Need to get a power of 2 - 1, 2, 4, 8, 16 etc
	// 2 ** (ceil(log2(needed_bytes)))
	const auto rounded_to_power_of_2 = static_cast<usize>(std::pow(2, ceil(log2(needed_bytes))));

	kind = concrete::Variant{ .type_tag_size = Bytes(rounded_to_power_of_2),
		                      .alternatives  = variant_types };
}

void valid_type::ValidType::defineFunction(
	const std::vector<ValidTypeID>& parameters, ValidTypeID result
) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;
	kind  = concrete::Function{ .parameters = parameters, .result = result };
}

void valid_type::ValidType::defineOpaque(Bytes size) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;
	kind  = concrete::Opaque{ size };
}

void valid_type::ValidType::finalizeStructureInheritanceMetadata(
	ValidTypeMap& types, concrete::Structure& structure
) {
	if (!structure.inheritance_metadata.has_value()) return;
	const auto& prev_imd = *structure.inheritance_metadata;

	// In this case we are finalizing a class or an interface, because only they
	// have inheritance metadata. We need to restore to forwarded data.
	// Note that we are copying the data here, it could be optimized in the future.
	const auto& implements          = prev_imd.implements;
	const auto& new_virtual_methods = prev_imd.available_methods;
	const auto& implementations     = prev_imd.vtable;

	// 1. Finalize superclass and interfaces first.
	for (auto& i: implements) types.at(i)->finalize(types);
	variant_match(prev_imd.kind) {
		variant_case(concrete::InheritanceMetadata::ClassKind, class_kind) {
			if_opt_some(class_kind.extends, superclass_id) {
				types.at(superclass_id)->finalize(types);
			}
		}
	}

	// 2. Fill the data
	std::vector<std::pair<base::StrID, valid_type::ValidTypeID>> fields;
	concrete::InheritanceMetadata                                imd = { .super_types = { getID() },
		                                                                 .implements  = implements,
		                                                                 .available_methods = {},
		                                                                 .vtable            = {},
		                                                                 .kind = prev_imd.kind };

	variant_match(prev_imd.kind) {
		variant_case(concrete::InheritanceMetadata::ClassKind, class_kind) {
			match_optional(class_kind.extends) {
				opt_some(superclass_id) {
					auto superclass = types.at(superclass_id);
					CORE_ASSERT(
						superclass->isKind<concrete::Structure>(), "Superclass must be a structure"
					);
					const auto& super_structure = superclass->getKindAs<concrete::Structure>();
					const auto& super_imd       = super_structure.inheritance_metadata.expect(
                        "Superclass must have inheritance metadata"
                    );

					// Fields. Superclass fields are inserted before subclass fields.
					for (const auto& field: superclass->getKindAs<concrete::Structure>().fields)
						fields.emplace_back(field.name, field.type);
					// Super types
					imd.super_types.insert(
						superclass->getKindAs<concrete::Structure>()
							.inheritance_metadata->super_types.begin(),
						superclass->getKindAs<concrete::Structure>()
							.inheritance_metadata->super_types.end()
					);
					// Virtual methods
					for (const auto& method: super_imd.available_methods)
						imd.available_methods.put(method.first, method.second);
					// Vtable
					for (const auto& impl: super_imd.vtable)
						imd.vtable.put(impl.first, impl.second);
				}


				opt_none {
					fields.emplace_back(
						base::StrID(".vtable"),
						types.at(typeName(SpecialTypes::get().vtable_ptr))->getID()
					);
				}
			}
		}
	}


	// For all interfaces, add virtual_methods and vtable entries, and add their
	// super types to this type's super types.
	for (auto& i: implements) {
		auto interface = types.at(i);
		CORE_ASSERT(interface->isKind<concrete::Structure>(), "Interface must be a structure");
		const auto& interface_structure = interface->getKindAs<concrete::Structure>();
		const auto& interface_imd       = interface_structure.inheritance_metadata.expect(
            "Interface must have inheritance metadata"
        );

		// Super types
		imd.super_types.insert(interface_imd.super_types.begin(), interface_imd.super_types.end());
		// Virtual methods
		for (const auto& method: interface_imd.available_methods)
			imd.available_methods.put(method.first, method.second);
		// Vtable
		for (const auto& impl: interface_imd.vtable) imd.vtable.put(impl.first, impl.second);
	}

	// 3. Add fields from this class and build inheritance metadata for this class.
	// Fields
	for (const auto& new_field: structure.fields)
		fields.emplace_back(new_field.name, new_field.type);
	// Virtual methods
	for (const auto& new_virtual_method: new_virtual_methods)
		imd.available_methods.insertOrAssign(new_virtual_method.first, new_virtual_method.second);
	// Vtable
	for (const auto& impl: implementations) imd.vtable.insertOrAssign(impl.first, impl.second);

	// 4. Put the data into the type
	structure.fields.clear();
	for (const auto& [field_name, field_type]: fields)
		structure.fields.insert(
			concrete::Field{ .offset = valid_type::TypeSize(Bytes(0), 0),
		                     .name   = field_name,
		                     .type   = field_type },
			field_name
		);
	std::get<concrete::Structure>(kind).inheritance_metadata = imd;
}

void valid_type::ValidType::finalize(ValidTypeMap& types) {
	switch (state) {
	case State::Declared:
		CORE_PANIC("Tried to finalize a type that was not defined");
	case State::Defined:
		state = State::Finalizing;
		break;
	case State::Finalized:
		return;
	case State::Finalizing:
		CORE_PANIC("Cyclic dependency not detected during type validation");
	}
	state = State::Finalizing;

	variant_match(kind) {
		variant_case(concrete::Primitive, primitive) {
			this->size                  = valid_type::TypeSize(primitive.size, 0);
			this->is_trivially_copyable = true;
		}
		variant_case(concrete::Pointer, pointer) {
			this->size                  = valid_type::TypeSize::pointer();
			this->is_trivially_copyable = true;
		}
		variant_case(concrete::FixedSizeTable, fixed_size_table) {
			auto inner_type = types.at(fixed_size_table.inner);
			inner_type->finalize(types);
			this->size                  = inner_type->getSize() * fixed_size_table.element_count;
			this->is_trivially_copyable = inner_type->isTriviallyCopyable();
		}
		variant_case(concrete::DynamicTable, dynamic_table) {
			/// @note Size of dynamicTable is unknown at this point,
			/// as this is a runtime property, so size should never be queried,
			/// through this object. Dynamic table is only accessed by a pointer.
			this->size                  = valid_type::TypeSize(Bytes(0), 0);
			this->is_trivially_copyable = false;
		}
		variant_case(concrete::Opaque, opaque) {
			// Opaque type size is known, so we don't need to do anything here.
			this->size                  = valid_type::TypeSize(opaque.size, 0);
			this->is_trivially_copyable = true;
		}
		variant_case(concrete::Function, function) {
			// Function type size is known, so we don't need to do anything here.
			this->size                  = valid_type::TypeSize::pointer();
			this->is_trivially_copyable = false;
		}

		variant_case(concrete::Variant, variant) {
			// Finalize the alternatives and calculate the size of the variant.
			// The process to calculate data_segment_size is not trivial,
			// because technically it's the maximum of the sizes of the alternatives, but we also
			// need to take into account different pointer sizes. More info in TypeSize's doc-comment.
			valid_type::TypeSize data_segment_size(Bytes(0), 0);
			for (auto& alternative: variant.alternatives) {
				auto alternative_type = types.at(alternative);
				alternative_type->finalize(types);
				data_segment_size = data_segment_size.fieldMax(alternative_type->getSize());
			}
			this->size = valid_type::TypeSize(Bytes(variant.type_tag_size), 0) + data_segment_size;
			this->is_trivially_copyable = false;
		}

		variant_case(concrete::Structure, structure) {
			finalizeStructureInheritanceMetadata(types, structure);

			// Finalize the fields and calculate their offsets.
			valid_type::TypeSize offset(Bytes(0), 0);
			for (auto& field: structure.fields) {
				auto field_type = types.at(field.type);
				field_type->finalize(types);
				field.offset = offset;
				offset += field_type->getSize();
			}
			this->size = offset;
			this->is_trivially_copyable
				= std::ranges::all_of(structure.fields, [&](const auto& field) {
					  auto field_type = types.at(field.type);
					  return field_type->isTriviallyCopyable();
				  });
		}
		variant_default { CORE_PANIC("Finalization not implemented for this type kind"); }
	}

	finalizeInstantiability(types);
	state = State::Finalized;
}

[[nodiscard]] base::StrID valid_type::ValidType::getName() const { return name; }

[[nodiscard]] valid_type::ValidTypeID valid_type::ValidType::getID() const { return id; }

bool valid_type::ValidType::operator==(const ValidType& other) const { return other.id == id; }

bool valid_type::ValidType::operator==(const ValidTypeID& other_id) const { return id == other_id; }

valid_type::ValidType::ValidType(base::StrID name, ValidTypeID id): name(name), id(id) {}

void valid_type::ValidType::finalizeInstantiability(ValidTypeMap& types) {
	// This method assumes all dependent types are already finalized, so we can query their
	// instantiability.
	variant_match(kind) {
		variant_case(concrete::Primitive, primitive) {
			if (name == "void") is_instantiable = false;
		}
		variant_case(concrete::Pointer, pointer) {
			// Any pointer is instantiable.
			is_instantiable = true;
		}
		variant_case(concrete::FixedSizeTable, fixed_size_table) {
			auto inner_type = types.at(fixed_size_table.inner);
			is_instantiable = inner_type->isInstantiable();
		}
		variant_case(concrete::DynamicTable, dynamic_table) { is_instantiable = false; }
		variant_case(concrete::Structure, structure) {
			is_instantiable = std::ranges::all_of(structure.fields, [&](const auto& field) {
				auto field_type = types.at(field.type);
				return field_type->isInstantiable();
			});
			const bool is_abstract
				= structure.inheritance_metadata
			          .map([](const auto& imd) {
						  variant_match(imd.kind) {
							  variant_case(concrete::InheritanceMetadata::ClassKind, class_kind) {
								  return class_kind.is_abstract;
							  }
							  variant_case(
								  concrete::InheritanceMetadata::InterfaceKind, interface_kind
							  ) {
								  return true;
							  }
						  }
						  CORE_UNREACHABLE();
					  })
			          .copyValueOr(false);
			is_instantiable &= !is_abstract;
		}
		variant_case(concrete::Variant, variant) {
			is_instantiable
				= std::ranges::all_of(variant.alternatives, [&](const auto& alternative) {
					  auto alternative_type = types.at(alternative);
					  return alternative_type->isInstantiable();
				  });
		}
		variant_case(concrete::Function, function) { is_instantiable = true; }
		variant_case(concrete::Opaque, opaque) {
			// Opaque types are instantiable, because otherwise there would be no way to call a
			// function that takes or returns an opaque. They are however immutable, because there
			// are not operations on opaque types except copying and passing to functions.
			is_instantiable = true;
		}
	}
}

[[nodiscard]] valid_type::TypeSize valid_type::ValidType::getSize() const {
	CORE_ASSERT(state == State::Finalized, "Tried to get size of a type that was not finalized");
	CORE_ASSERT(
		!std::holds_alternative<concrete::DynamicTable>(kind), "Tried to get size of a dynamic table"
	);
	return size;
}

[[nodiscard]]
bool valid_type::ValidType::isInstantiable() const {
	CORE_ASSERT(
		state == State::Finalized, "Tried to query instantiability of a type that was not finalized"
	);
	return is_instantiable;
}

[[nodiscard]] valid_type::ConcreteTypeVariant valid_type::ValidType::getKind() const {
	return kind;
}

[[nodiscard]] bool vm::code::valid_type::ValidType::isTriviallyCopyable() const {
	return is_trivially_copyable;
}
