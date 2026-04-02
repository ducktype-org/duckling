#include "valid_type.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <algorithm>
#include <variant>

using namespace vm::code;

valid_type::ValidType valid_type::ValidType::declareType(base::StrID name, ValidTypeID id) {
	return { name, id };
}

void valid_type::ValidType::definePrimitive(Bytes size) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			state = Defined{ .kind = defined::DefinedPrimitive{ size } };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::definePointer(ValidTypeID inner) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			state = Defined{ .kind = defined::DefinedPointer{ inner } };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::defineFixedSizeTable(ValidTypeID inner, usize element_count) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			state = Defined{ .kind = defined::DefinedFixedSizeTable{
								 .inner = inner, .element_count = element_count } };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::defineDynamicTable(ValidTypeID inner) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			state = Defined{ .kind = defined::DefinedDynamicTable{ .inner = inner } };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::defineData(
	const std::vector<std::pair<base::StrID, ValidTypeID>>& fields_definitions
) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			std::vector<defined::DefinedField> fields;
			fields.reserve(fields_definitions.size());
			for (const auto& field_def: fields_definitions)
				fields.emplace_back(defined::DefinedField{ .name = field_def.first,
				                                           .type = field_def.second });
			state = Defined{ .kind = defined::DefinedStructure{
								 .field_definitions = fields, .forwarded_inheritance_data = {} } };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::defineClass(
	const std::vector<std::pair<base::StrID, ValidTypeID>>& fields_definitions,
	const bool                                              is_abstract,
	const base::Optional<ValidTypeID>&                      extends,
	const std::vector<ValidTypeID>&                         implements,
	const std::vector<std::pair<base::StrID, ValidTypeID>>& new_virtual_methods,
	const std::vector<std::pair<base::StrID, base::StrID>>& implementations
) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			auto structure = defined::DefinedStructure();
			for (const auto& field_def: fields_definitions)
				structure.field_definitions.emplace_back(defined::DefinedField{
					.name = field_def.first, .type = field_def.second });

			base::HashMap<base::StrID, ValidTypeID> new_virtual_methods_map;
			for (const auto& method: new_virtual_methods)
				new_virtual_methods_map.put(method.first, method.second);

			base::HashMap<base::StrID, base::StrID> implementations_map;
			for (const auto& impl: implementations)
				implementations_map.put(impl.first, impl.second);

			structure.forwarded_inheritance_data = defined::InheritanceDefinitionData{
				.implements          = implements | std::ranges::to<std::unordered_set>(),
				.new_virtual_methods = new_virtual_methods_map,
				.implementations = implementations_map,
				.kind = defined::InheritanceDefinitionData::ClassKind{
					.extends     = extends,
					.is_abstract = is_abstract,
				},
			};
			state = Defined{ .kind = std::move(structure) };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::defineInterface(
	const std::vector<ValidTypeID>&                         implements,
	const std::vector<std::pair<base::StrID, ValidTypeID>>& new_virtual_methods,
	const std::vector<std::pair<base::StrID, base::StrID>>& implementations
) {
	variant_match(state) {
		variant_case_novalue(Declared) {
			base::HashMap<base::StrID, ValidTypeID> virtual_methods;
			for (const auto& method: new_virtual_methods)
				virtual_methods.put(method.first, method.second);

			base::HashMap<base::StrID, base::StrID> impls;
			for (const auto& impl: implementations) impls.put(impl.first, impl.second);

			state
				= Defined{ .kind = defined::DefinedStructure{
							   .field_definitions = {},
							   .forwarded_inheritance_data
							   = defined::InheritanceDefinitionData{ .implements = implements
				                                                     | std::ranges::to<std::unordered_set>(),
				                                                     .new_virtual_methods
				                                                     = virtual_methods,
				                                                     .implementations = impls,
				                                                     .kind
				                                                     = defined::InheritanceDefinitionData::
				                                                         InterfaceKind(), },
						   }, };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::defineVariant(const std::vector<ValidTypeID>& variant_types) {
	CORE_ASSERT(!variant_types.empty(), "Cannot define variant with no alternatives");
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			state = Defined{ .kind = defined::DefinedVariant{ .alternatives = variant_types } };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::defineFunction(
	const std::vector<ValidTypeID>& parameters, const std::vector<ValidTypeID>& result
) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			state = Defined{ .kind = defined::DefinedFunction{ .parameters = parameters,
				                                               .result     = result } };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

void valid_type::ValidType::defineOpaque(Bytes size) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			state = Defined{ .kind = defined::DefinedOpaque{ .size = size } };
		}
		variant_default { CORE_PANIC("Bad type define: type already defined or finalized"); }
	}
}

valid_type::finalized::Structure valid_type::ValidType::finalizeStructureData(
	ValidTypeMap& types, const defined::DefinedStructure& structure
) const {
	std::vector<std::pair<base::StrID, valid_type::ValidTypeID>> fields;
	base::Optional<finalized::InheritanceMetadata>               inheritance_metadata;
	if (!structure.forwarded_inheritance_data.has_value()) {
		// In this case we are finalizing a structure that does not have inheritance metadata, so
		// we just fill the fields from the structure definition.
		for (const auto& field: structure.field_definitions)
			fields.emplace_back(field.name, field.type);
	} else {
		const auto& prev_imd = *structure.forwarded_inheritance_data;

		// In this case we are finalizing a class or an interface, because only they
		// have inheritance metadata. We need to restore to forwarded data.
		// Note that we are not copying the data here
		const auto& implements          = prev_imd.implements;
		const auto& new_virtual_methods = prev_imd.new_virtual_methods;
		const auto& implementations     = prev_imd.implementations;

		// 1. Finalize superclass and interfaces first.
		for (auto& i: implements) types.at(i)->finalize(types);
		variant_match(prev_imd.kind) {
			variant_case(defined::InheritanceDefinitionData::ClassKind, class_kind) {
				if_opt_some(class_kind.extends, superclass_id) {
					types.at(superclass_id)->finalize(types);
				}
			}
		}

		// 2. Fill the data
		const auto translated_kind = std::visit(
			[](const auto& kind) -> decltype(finalized::InheritanceMetadata::kind) {
				using T = std::decay_t<decltype(kind)>;
				if constexpr (std::is_same_v<T, defined::InheritanceDefinitionData::ClassKind>) {
					return finalized::InheritanceMetadata::ClassKind{
						.extends     = kind.extends,
						.is_abstract = kind.is_abstract,
					};
				} else if constexpr (std::is_same_v<
										 T,
										 defined::InheritanceDefinitionData::InterfaceKind>) {
					return finalized::InheritanceMetadata::InterfaceKind{};
				}
			},
			prev_imd.kind
		);

		finalized::InheritanceMetadata imd = { .super_types       = { getID() },
			                                   .implements        = implements,
			                                   .available_methods = {},
			                                   .vtable            = {},
			                                   .kind              = translated_kind };

		variant_match(prev_imd.kind) {
			variant_case(defined::InheritanceDefinitionData::ClassKind, class_kind) {
				match_optional(class_kind.extends) {
					opt_some(superclass_id) {
						auto superclass = types.at(superclass_id);
						CORE_ASSERT(
							superclass->isKind<finalized::Structure>(),
							"Superclass must be a structure"
						);
						const auto  super_structure = superclass->getKindAs<finalized::Structure>();
						const auto& super_imd       = super_structure->inheritance_metadata.expect(
                            "Superclass must have inheritance metadata"
                        );

						// Fields. Superclass fields are inserted before subclass fields.
						for (const auto& field:
						     superclass->getKindAs<finalized::Structure>()->fields)
							fields.emplace_back(field.name, field.type);
						// Super types
						imd.super_types.insert(
							superclass->getKindAs<finalized::Structure>()
								->inheritance_metadata->super_types.begin(),
							superclass->getKindAs<finalized::Structure>()
								->inheritance_metadata->super_types.end()
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
			CORE_ASSERT(interface->isKind<finalized::Structure>(), "Interface must be a structure");
			const auto& interface_structure = interface->getKindAs<finalized::Structure>();
			const auto& interface_imd       = interface_structure->inheritance_metadata.expect(
                "Interface must have inheritance metadata"
            );

			// Super types
			imd.super_types.insert(
				interface_imd.super_types.begin(), interface_imd.super_types.end()
			);
			// Virtual methods
			for (const auto& method: interface_imd.available_methods)
				imd.available_methods.put(method.first, method.second);
			// Vtable
			for (const auto& impl: interface_imd.vtable) imd.vtable.put(impl.first, impl.second);
		}

		// 3. Add fields from this class and build inheritance metadata for this class.
		// Fields
		for (const auto& new_field: structure.field_definitions)
			fields.emplace_back(new_field.name, new_field.type);
		// Virtual methods
		for (const auto& new_virtual_method: new_virtual_methods)
			imd.available_methods.insertOrAssign(
				new_virtual_method.first, new_virtual_method.second
			);
		// Vtable
		for (const auto& impl: implementations) imd.vtable.insertOrAssign(impl.first, impl.second);

		inheritance_metadata = std::move(imd);
	}

	// 4. Put the data into the type
	finalized::Structure new_structure;
	for (const auto& [field_name, field_type]: fields)
		new_structure.fields.insert(
			finalized::Field{ .offset = valid_type::TypeSize(Bytes(0), 0),
		                      .name   = field_name,
		                      .type   = field_type },
			field_name
		);
	new_structure.inheritance_metadata = std::move(inheritance_metadata);

	return new_structure;
}

void valid_type::ValidType::finalize(ValidTypeMap& types) {
	variant_match(state) {
		variant_case_novalue(ValidType::Declared) {
			CORE_PANIC("Tried to finalize a type that was not defined: " + base::toString(name));
		}
		variant_case_novalue(ValidType::Finalized) { return; }
		variant_case_novalue(ValidType::Finalizing) {
			CORE_PANIC("Cyclic dependency not detected during type validation");
		}
		variant_case(Defined, defined) { state = Finalizing{ std::move(defined.kind) }; }
	}

	variant_match(std::get<Finalizing>(state).kind) {
		variant_case(defined::DefinedPrimitive, primitive) {
			this->size                  = valid_type::TypeSize(primitive.size, 0);
			this->is_trivially_copyable = true;
			state = Finalized{ .kind = finalized::Primitive{ primitive.size } };
		}
		variant_case(defined::DefinedPointer, pointer) {
			this->size                  = valid_type::TypeSize::pointer();
			this->is_trivially_copyable = false;
			state                       = Finalized{ .kind = finalized::Pointer{ pointer.inner } };
		}
		variant_case(defined::DefinedFixedSizeTable, fixed_size_table) {
			auto inner_type = types.at(fixed_size_table.inner);
			inner_type->finalize(types);
			this->size                  = inner_type->getSize() * fixed_size_table.element_count;
			this->is_trivially_copyable = inner_type->isTriviallyCopyable();
			state                       = Finalized{ .kind = finalized::FixedSizeTable{
														 .inner         = fixed_size_table.inner,
														 .element_count = fixed_size_table.element_count } };
		}
		variant_case(defined::DefinedDynamicTable, dynamic_table) {
			/// @note Size of dynamicTable is unknown at this point,
			/// as this is a runtime property, so size should never be queried,
			/// through this object. Dynamic table is only accessed by a pointer.
			this->size                  = valid_type::TypeSize(Bytes(0), 0);
			this->is_trivially_copyable = false;
			state = Finalized{ .kind = finalized::DynamicTable{ .inner = dynamic_table.inner } };
		}
		variant_case(defined::DefinedOpaque, opaque) {
			// Opaque type size is known, so we don't need to do anything here.
			this->size                  = valid_type::TypeSize(opaque.size, 0);
			this->is_trivially_copyable = true;
			state                       = Finalized{ .kind = finalized::Opaque{ opaque.size } };
		}
		variant_case(defined::DefinedFunction, function) {
			// Function type size is known, so we don't need to do anything here.
			this->size                  = valid_type::TypeSize::pointer();
			this->is_trivially_copyable = false;
			state                       = Finalized{ .kind
                               = finalized::Function{ .parameters = std::move(function.parameters),
				                                                            .result     = std::move(function.result) } };
		}
		variant_case(defined::DefinedVariant, variant) {
			CORE_ASSERT(
				variant.alternatives.size() >= 2, "Variant must have at least 2 alternatives"
			);
			const auto num_alternatives = variant.alternatives.size();
			const auto needed_bits      = static_cast<usize>(std::bit_width(num_alternatives - 1));
			// Add 7 so we round up to the nearest byte, because we cannot have sub-byte sizes.
			const auto needed_bytes          = (needed_bits + 7) / 8;
			const auto rounded_to_power_of_2 = std::bit_ceil(needed_bytes);
			const auto type_tag_size         = Bytes(rounded_to_power_of_2);

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
			this->size = valid_type::TypeSize(type_tag_size, 0) + data_segment_size;
			this->is_trivially_copyable = false;
			state                       = Finalized{ .kind = finalized::Variant{
														 .type_tag_size        = type_tag_size,
														 .alternatives_ordered = variant.alternatives,
														 .alternatives_set
                                   = variant.alternatives | std::ranges::to<std::unordered_set>(),
                               } };
		}
		variant_case(defined::DefinedStructure, structure) {
			auto new_structure = finalizeStructureData(types, structure);

			// Finalize the fields and calculate their offsets.
			valid_type::TypeSize offset(Bytes(0), 0);
			for (auto& field: new_structure.fields) {
				auto field_type = types.at(field.type);
				field_type->finalize(types);
				field.offset = offset;
				offset += field_type->getSize();
			}
			this->size = offset;
			this->is_trivially_copyable
				= std::ranges::all_of(new_structure.fields, [&](const auto& field) {
					  auto field_type = types.at(field.type);
					  return field_type->isTriviallyCopyable();
				  });
			state = Finalized{ .kind = std::move(new_structure) };
		}
		variant_default { CORE_PANIC("Finalization not implemented for this type kind"); }
	}
	CORE_ASSERT(
		std::holds_alternative<Finalized>(state),
		"Type finalization failed to set state to Finalized"
	);

	finalizeInstantiability(types);
}

[[nodiscard]] base::StrID valid_type::ValidType::getName() const { return name; }

[[nodiscard]] valid_type::ValidTypeID valid_type::ValidType::getID() const { return id; }

bool valid_type::ValidType::operator==(const ValidType& other) const { return other.id == id; }

bool valid_type::ValidType::operator==(const ValidTypeID& other_id) const { return id == other_id; }

valid_type::ValidType::ValidType(base::StrID name, ValidTypeID id): name(name), id(id) {}

void valid_type::ValidType::finalizeInstantiability(ValidTypeMap& types) {
	// This method assumes all dependent types are already finalized, so we can query their
	// instantiability.
	variant_match(getKind()) {
		variant_case_novalue(finalized::Primitive) {
			is_instantiable = true;
		}
		variant_case(finalized::Pointer, pointer) {
			// Any pointer is instantiable.
			is_instantiable = true;
		}
		variant_case(finalized::FixedSizeTable, fixed_size_table) {
			auto inner_type = types.at(fixed_size_table.inner);
			is_instantiable = inner_type->isInstantiable();
		}
		variant_case(finalized::DynamicTable, dynamic_table) { is_instantiable = false; }
		variant_case(finalized::Structure, structure) {
			is_instantiable = std::ranges::all_of(structure.fields, [&](const auto& field) {
				auto field_type = types.at(field.type);
				return field_type->isInstantiable();
			});
			const bool is_abstract
				= structure.inheritance_metadata
			          .map([](const auto& imd) {
						  variant_match(imd.kind) {
							  variant_case(finalized::InheritanceMetadata::ClassKind, class_kind) {
								  return class_kind.is_abstract;
							  }
							  variant_case(
								  finalized::InheritanceMetadata::InterfaceKind, interface_kind
							  ) {
								  return true;
							  }
						  }
						  CORE_UNREACHABLE();
					  })
			          .copyValueOr(false);
			is_instantiable &= !is_abstract;
		}
		variant_case(finalized::Variant, variant) {
			is_instantiable
				= std::ranges::all_of(variant.alternatives_ordered, [&](const auto& alternative) {
					  auto alternative_type = types.at(alternative);
					  return alternative_type->isInstantiable();
				  });
		}
		variant_case(finalized::Function, function) { is_instantiable = true; }
		variant_case(finalized::Opaque, opaque) {
			// Opaque types are instantiable, because otherwise there would be no way to call a
			// function that takes or returns an opaque. They are however immutable, because there
			// are not operations on opaque types except copying and passing to functions.
			is_instantiable = true;
		}
	}
}

[[nodiscard]] valid_type::TypeSize valid_type::ValidType::getSize() const {
	variant_match(state) {
		variant_case(Finalized, finalized) {
			CORE_ASSERT(
				!std::holds_alternative<finalized::DynamicTable>(finalized.kind),
				"Tried to get size of a dynamic table"
			);
			return size;
		}
		variant_default { CORE_PANIC("Tried to get size of a type that was not finalized"); }
	}
}

[[nodiscard]]
bool valid_type::ValidType::isInstantiable() const {
	variant_match(state) {
		variant_case(Finalized, finalized) { return is_instantiable; }
		variant_default {
			CORE_PANIC("Tried to query instantiability of a type that was not finalized");
		}
	}
}

[[nodiscard]] valid_type::FinalizedTypeVariant valid_type::ValidType::getKind() const {
	variant_match(state) {
		variant_case(Finalized, finalized) { return finalized.kind; }
		variant_default { CORE_PANIC("Tried to get kind of a type that was not finalized"); }
	}
}

[[nodiscard]] bool vm::code::valid_type::ValidType::isTriviallyCopyable() const {
	variant_match(state) {
		variant_case_novalue(Finalized) { return is_trivially_copyable; }
		variant_default {
			CORE_PANIC("Tried to query trivial copyability of a type that was not finalized");
		}
	}
}
