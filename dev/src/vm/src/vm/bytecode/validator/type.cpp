#include "type.hpp"

#include "base/except/exceptions.hpp"
#include <base/collections/optional.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <algorithm>
#include <cmath>
#include <variant>

using namespace vm::code;

type::Type type::Type::declareType(base::StrID name, TypeID id) { return { name, id }; }

void type::Type::definePrimitive(Bytes size) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind       = concrete::Primitive{ size };
	this->size = type::TypeSize(size, 0);
}

void type::Type::definePointer(type::TypeID inner) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind       = concrete::Pointer{ inner };
	this->size = type::TypeSize::pointer();
}

void type::Type::defineFixedSizeTable(type::TypeID inner, usize element_count) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind = concrete::FixedSizeTable{ .inner = inner, .element_count = element_count };
}

void type::Type::defineDynamicTable(type::TypeID inner) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind = concrete::DynamicTable{ .inner = inner };

	/// @note Size of dynamicTable is unknown at this point,
	/// as this is a runtime property. Though it should never be accessed,
	/// because dynamic table is only accessed by a pointer.
	this->size = type::TypeSize(Bytes(0), 0);
}

void type::Type::defineData(
	const std::vector<std::pair<base::StrID, type::TypeID>>& fields_definitions
) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	ObjIdNameMap<concrete::Field, concrete::Field::ID> fields;
	for (const auto& field_def: fields_definitions)
		fields.insert(
			concrete::Field{ .offset = type::TypeSize(Bytes(0), 0),  // Offsets are calculated later
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

void type::Type::defineClass(
	const std::vector<std::pair<base::StrID, type::TypeID>>& fields_definitions,
	const bool                                               is_abstract,
	const base::Optional<type::TypeID>&                      extends,
	const std::vector<type::TypeID>&                         implements,
	const std::vector<std::pair<base::StrID, type::TypeID>>& new_virtual_methods,
	const std::vector<std::pair<base::StrID, base::StrID>>&  implementations
) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind = concrete::Structure();
	for (const auto& field_def: fields_definitions)
		std::get<concrete::Structure>(kind).fields.insert(
			concrete::Field{ .offset = type::TypeSize(Bytes(0), 0),
		                     .name   = field_def.first,
		                     .type   = field_def.second },
			field_def.first
		);

	base::HashMap<base::StrID, type::TypeID> virtual_methods;
	for (const auto& method: new_virtual_methods) virtual_methods.put(method.first, method.second);

	base::HashMap<base::StrID, base::StrID> vtable;
	for (const auto& impl: implementations) vtable.put(impl.first, impl.second);

	// Forward the data to be used in inheritance metadata construction during finalization.
	std::get<concrete::Structure>(kind).inheritance_metadata = concrete::InheritanceMetadata {
		.super_types       = { },
		.implements        = implements | std::ranges::to<std::unordered_set>(),
		.available_methods = virtual_methods, .vtable = vtable,
		.kind = concrete::InheritanceMetadata::ClassKind{
			.extends     = extends,
			.is_abstract = is_abstract,
		},
	};
}

void type::Type::defineInterface(
	const std::vector<type::TypeID>&                         implements,
	const std::vector<std::pair<base::StrID, type::TypeID>>& new_virtual_methods,
	const std::vector<std::pair<base::StrID, base::StrID>>&  implementations
) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind = concrete::Structure();

	base::HashMap<base::StrID, type::TypeID> virtual_methods;
	for (const auto& method: new_virtual_methods) virtual_methods.put(method.first, method.second);

	base::HashMap<base::StrID, base::StrID> vtable;
	for (const auto& impl: implementations) vtable.put(impl.first, impl.second);

	// Forward the data to be used in inheritance metadata construction during finalization.
	std::get<concrete::Structure>(kind).inheritance_metadata
		= concrete::InheritanceMetadata{ .super_types = {},
		                                 .implements
		                                 = implements | std::ranges::to<std::unordered_set>(),
		                                 .available_methods = virtual_methods,
		                                 .vtable            = vtable,
		                                 .kind = concrete::InheritanceMetadata::InterfaceKind() };
}

void type::Type::defineVariant(const std::vector<TypeID>& variant_types) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	CORE_ASSERT(variant_types.size() != 0, "Cannot define variant with no alternatives");
	state = State::Defined;

	// log_256(x) = log_2(x) / log_2(256) = log_2(x) / 8.0
	const auto needed_bytes = ceil(log2(static_cast<double>(variant_types.size())) / 8.0);

	// Need to get a power of 2 - 1, 2, 4, 8, 16 etc
	// 2 ** (ceil(log2(needed_bytes)))
	const auto rounded_to_power_of_2 = static_cast<usize>(std::pow(2, ceil(log2(needed_bytes))));

	kind = concrete::Variant{ .type_tag_size = Bytes(rounded_to_power_of_2),
		                      .alternatives  = variant_types };
}

void type::Type::defineFunction(const std::vector<TypeID>& parameters, TypeID result) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind       = concrete::Function{ .parameters = parameters, .result = result };
	this->size = type::TypeSize::pointer();
}

void type::Type::defineOpaque(Bytes size) {
	CORE_ASSERT(state == State::Declared, "Bad type define");
	state = State::Defined;

	kind       = concrete::Opaque{ size };
	this->size = type::TypeSize(size, 0);
}

void type::Type::finalize(ObjIdNameMap<type::Type>& types) {
	switch (state) {
	case State::Declared:
		CORE_PANIC("Tried to finalize a type that was not defined");
	case State::Defined:
		state = State::Finalizing;
		break;
	case State::Finalizing:
		CORE_PANIC("Cyclic dependency not detected during type validation");
	case State::Finalized:
		return;
	}
	state = State::Finalizing;

	variant_match(kind) {
		variant_case(concrete::FixedSizeTable, fixed_size_table) {
			auto inner_type = types.at(fixed_size_table.inner);
			inner_type->finalize(types);
			this->size = inner_type->getSize() * fixed_size_table.element_count;
		}
		variant_case(concrete::Pointer, pointer) {
			// Pointer size is not known at this point, because it can be different in safe and fast
			// modes. We just need to finalize the inner type, to make sure there are no cyclic
			// dependencies.
			types.at(pointer.inner)->finalize(types);
		}
		variant_case(concrete::Opaque, opaque) {
			// Opaque type size is known, so we don't need to do anything here.
		}
		variant_case(concrete::Function, function) {
			// Function type size is known, so we don't need to do anything here.
		}
		variant_case(concrete::Primitive, primitive) {
			// Primitive type size is known, so we don't need to do anything here.
		}

		variant_case(concrete::Variant, variant) {
			// Finalize the alternatives and calculate the size of the variant.
			// The process to calculate data_segment_size is not trivial,
			// because technically it's the maximum of the sizes of the alternatives, but we also
			// need to take into account different pointer sizes. More info in TypeSize's doc-comment.
			type::TypeSize data_segment_size(Bytes(0), 0);
			for (auto& alternative: variant.alternatives) {
				auto alternative_type = types.at(alternative);
				alternative_type->finalize(types);
				data_segment_size = data_segment_size.fieldMax(alternative_type->getSize());
			}
			this->size = type::TypeSize(Bytes(variant.type_tag_size), 0) + data_segment_size;
		}

		variant_case(concrete::Structure, structure) {
			if_opt_some(structure.inheritance_metadata, prev_imd) {
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
				std::vector<std::pair<base::StrID, type::TypeID>> fields;
				concrete::InheritanceMetadata imd = { .super_types       = { getId() },
					                                  .implements        = implements,
					                                  .available_methods = {},
					                                  .vtable            = {},
					                                  .kind              = prev_imd.kind };

				variant_match(prev_imd.kind) {
					variant_case(concrete::InheritanceMetadata::ClassKind, class_kind) {
						match_optional(class_kind.extends) {
							opt_some(superclass_id) {
								auto superclass = types.at(superclass_id);
								CORE_ASSERT(
									superclass->is<concrete::Structure>(),
									"Superclass must be a structure"
								);
								const auto& super_structure
									= superclass->get<concrete::Structure>();
								const auto& super_imd = super_structure.inheritance_metadata.expect(
									"Superclass must have inheritance metadata"
								);

								// Fields. Superclass fields are inserted before subclass fields.
								for (const auto& field:
								     superclass->get<concrete::Structure>().fields)
									fields.emplace_back(field.name, field.type);
								// Super types
								imd.super_types.insert(
									superclass->get<concrete::Structure>()
										.inheritance_metadata->super_types.begin(),
									superclass->get<concrete::Structure>()
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
									types.at(typeName(SpecialTypes::get().vtable_ptr))->getId()
								);
							}
						}
					}

					// For all interfaces, add virtual_methods and vtable entries, and add their
					// super types to this type's super types.
					for (auto& i: implements) {
						auto interface = types.at(i);
						CORE_ASSERT(
							interface->is<concrete::Structure>(), "Interface must be a structure"
						);
						const auto& interface_structure = interface->get<concrete::Structure>();
						const auto& interface_imd = interface_structure.inheritance_metadata.expect(
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
						for (const auto& impl: interface_imd.vtable)
							imd.vtable.put(impl.first, impl.second);
					}


					// 3. Add fields from this class and build inheritance metadata for this class.
					// Fields
					for (const auto& new_field: structure.fields)
						fields.emplace_back(new_field.name, new_field.type);
					// Virtual methods
					for (const auto& new_virtual_method: new_virtual_methods)
						imd.available_methods.put(
							new_virtual_method.first, new_virtual_method.second
						);
					// Vtable
					for (const auto& impl: implementations) imd.vtable.put(impl.first, impl.second);

					// 4. Put the data into the type
					structure.fields.clear();
					for (const auto& [field_name, field_type]: fields)
						structure.fields.insert(
							concrete::Field{ .offset = type::TypeSize(Bytes(0), 0),
						                     .name   = field_name,
						                     .type   = field_type },
							field_name
						);
					std::get<concrete::Structure>(kind).inheritance_metadata = imd;
				}
			}

			// Finalize the fields and calculate their offsets.
			type::TypeSize offset(Bytes(0), 0);
			for (auto& field: structure.fields) {
				auto field_type = types.at(field.type);
				field_type->finalize(types);
				field.offset = offset;
				offset += field_type->getSize();
			}
			this->size = offset;
		}


		variant_default { CORE_PANIC("Finalization not implemented for this type kind"); }
	}

	finalizeInstantiability(types);
	state = State::Finalized;
}

[[nodiscard]] base::StrID type::Type::getName() const { return name; }

[[nodiscard]] type::TypeID type::Type::getId() const { return id; }

bool type::Type::operator==(const Type& other) const { return other.id == id; }

type::Type::Type(base::StrID name, TypeID id): name(name), id(id) {}

void type::Type::finalizeInstantiability(ObjIdNameMap<type::Type>& types) {
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

[[nodiscard]] type::TypeSize type::Type::getSize() const {
	CORE_ASSERT(state == State::Finalized, "Tried to get size of a type that was not finalized");
	CORE_ASSERT(
		!std::holds_alternative<concrete::DynamicTable>(kind), "Tried to get size of a dynamic table"
	);
	return size;
}

[[nodiscard]]
bool type::Type::isInstantiable() const {
	CORE_ASSERT(
		state == State::Finalized, "Tried to query instantiability of a type that was not finalized"
	);
	return is_instantiable;
}

[[nodiscard]] type::ConcreteTypeVariant type::Type::getKind() const { return kind; }
