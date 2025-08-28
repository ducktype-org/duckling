#include "type.hpp"

#include <bits/ranges_algo.h>

#include <base/defer.hpp>
#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/supervisor/supervisor.hpp>

#include <algorithm>
#include <utility>

namespace vm {
	void Type::isInstantiableImpl(kind::Data& data) {
		auto is_concrete_class = [](const InheritanceMetadata& imd) {
			variant_match(imd.kind) {
				variant_case(InheritanceMetadata::Class, clazz) { return !clazz.is_abstract; }
			}
			return false;
		};
		if_opt_some(data.inheritance_metadata, imd) {
			if (!is_concrete_class(imd)) {
				am_i_instantiable = false;
				return;
			}
		}

		for (auto& field: data.fields)
			if (!field.type->isInstantiable()) {
				am_i_instantiable = false;
				return;
			}

		return;
	}

	void Type::isInstantiableImpl(kind::Variant& variant) {
		for (auto& alt: variant.alternatives) {
			if (!alt->isInstantiable()) {
				am_i_instantiable = false;
				return;
			}
		}
	}

	void Type::inheritsFromImpl(InheritanceMetadata& imd) {
		imd.inherits_from.insert(getID());
		auto get_all_super = [](TypeCRef type) {
			return type->getInheritanceMetadata()
			    .expect("Deriving from type with no metadata")
			    ->inherits_from;
		};

		auto insert_all
			= [](auto& set, const auto& range) { set.insert(range.begin(), range.end()); };

		if_opt_some(getSuperClass(), super) insert_all(imd.inherits_from, get_all_super(super));
		for (auto i: imd.implements) insert_all(imd.inherits_from, get_all_super(i));
	}

	// Type declaration:
	Type Type::declareType(base::StrID name) {
		Type type{};
		type.name = name;
		return type;
	}

	// Type definition:
	void Type::definePrimitive(TypeSize pass_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type = Kind::Primitive;
		size      = pass_size;
		kind      = kind::Primitive();
	}

	void Type::definePointer(TypeCRef inner) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		size      = POINTER_SIZE;
		kind_type = Kind::Pointer;
		kind      = kind::Pointer{ inner };
	}

	void Type::defineFixedSizeTable(TypeRef inner, u64 table_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type = Kind::FixedSizeTable;
		kind      = kind::FixedSizeTable{ .inner_type = inner, .size = table_size };
	}

	void Type::defineDynamicTable(TypeRef inner) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type         = Kind::DynamicTable;
		kind              = kind::DynamicTable{ .inner_type = inner };
		am_i_instantiable = false;
	}

	void Type::defineData(
		const std::vector<std::pair<base::StrID, TypeRef>>& fields_definitions,
		base::Optional<InheritanceMetadata>                 inheritance_metadata
	) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type = Kind::Data;
		auto data = kind::Data{};
		for (auto [sub_name, sub_type]: fields_definitions) {
			data.field_name_map.put(sub_name, data.fields.size());
			// offset is set during finalization
			data.fields.emplace_back(kind::FieldDesc{ .offset = 0, .type = sub_type });
		}
		data.inheritance_metadata = std::move(inheritance_metadata);
		kind                      = data;
	}

	void Type::defineVariant(const std::vector<TypeRef>& variants_definitions) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type    = Kind::Variant;
		auto variant = kind::Variant{};
		for (const auto& type: variants_definitions) variant.alternatives.push_back(type);
		kind = variant;
	}

	void Type::defineFunction(std::vector<TypeCRef> parameters, TypeCRef result) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		size      = POINTER_SIZE;
		kind_type = Kind::Function;
		kind      = kind::Function{ .parameters = std::move(parameters), .result = result };
	}

	void Type::defineOpaque(TypeSize pass_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type = Kind::Opaque;
		size      = pass_size;
		kind      = kind::Opaque{};
	}

	void Type::finalize() {
		if (state == State::Finalizing) throw code::CyclicDependencyError(*this);
		if (state == State::Finalized) return;
		state = State::Finalizing;
		defer(state = State::Finalized);

		variant_match(kind) {
			variant_case(kind::FixedSizeTable, fixed_size_table) {
				fixed_size_table.inner_type->finalize();
				this->size = fixed_size_table.inner_type->getSize() * fixed_size_table.size;
			}
			variant_case(kind::Data, data) {
				// calculate offset and size
				Offset offset = 0;
				for (auto& field: data.fields) {
					field.offset = offset;
					field.type->finalize();
					offset += field.type->getSize();
				}
				this->size = offset;
				if_opt_some(data.inheritance_metadata, imd) { inheritsFromImpl(imd); }
				isInstantiableImpl(data);
			}
			variant_case(kind::Variant, variant) {
				// calculate size
				TypeSize data_size = 0;
				for (auto& alternative: variant.alternatives) {
					alternative->finalize();
					data_size = std::max(data_size, alternative->getSize());
				}
				this->size = 16 + data_size;
				isInstantiableImpl(variant);
			}
		}
	}

	// pointer, fixedSizeTable, dynamicTable
	base::Optional<TypeCRef> Type::getInnerType() const {
		auto get_inner_type = [](auto t) { return t->inner_type; };

		auto pointer_option = get<kind::Pointer>().map(get_inner_type);
		if (pointer_option.has_value()) return (TypeCRef) pointer_option.value();

		auto fixed_size_table_option = get<kind::FixedSizeTable>().map(get_inner_type);
		if (fixed_size_table_option.has_value()) return (TypeCRef) fixed_size_table_option.value();

		auto dynamic_table_option = get<kind::DynamicTable>().map(get_inner_type);
		if (dynamic_table_option.has_value()) return (TypeCRef) dynamic_table_option.value();

		return {};
	}

	// struct
	base::Optional<Offset> Type::getFieldOffsetByName(base::StrID field_name) const {
		return get<kind::Data>().flatMap(
			[field_name](CRef<kind::Data> data) -> base::Optional<Offset> {
				if_opt_some(data->field_name_map.atMaybe(field_name), field_index) {
					return data->fields[*field_index].offset;
				}
				return {};
			}
		);
	}

	// inheritance
	base::Optional<base::CRef<InheritanceMetadata>> Type::getInheritanceMetadata() const {
		variant_match(kind) {
			variant_case(kind::Data, data) {
				if (data.inheritance_metadata.has_value())
					return &data.inheritance_metadata.value();
			}
		}
		return {};
	}

	base::Optional<TypeCRef> Type::getSuperClass() const {
		variant_match(kind) {
			variant_case(kind::Data, data) {
				if_opt_some(data.inheritance_metadata, imd) {
					variant_match(imd.kind) {
						variant_case(InheritanceMetadata::Class, class_kind) {
							if (class_kind.extends.has_value()) return class_kind.extends.value();
						}
					}
				}
			}
		}
		return {};
	}

	bool Type::inheritsFrom(TypeCRef other) const {
		match_optional(getInheritanceMetadata()) {
			opt_some(imd) { return imd->inherits_from.contains(other->getID()); }
			opt_none { return false; }
		}
		CORE_UNREACHABLE();
	}

	bool Type::isInstantiable() const { return am_i_instantiable; }

	// function
	base::Optional<u64> Type::getParameterCount() const {
		return get<kind::Function>().map([](CRef<kind::Function> function) {
			return function->parameters.size();
		});
	}

	base::Optional<u64> Type::getParametersSize() const {
		return get<kind::Function>().map([](CRef<kind::Function> function) {
			usize size = 0;
			for (const auto& param: function->parameters) size += param->getSize();
			return size;
		});
	}

	base::Optional<TypeCRef> Type::getNthParameterType(u64 parameter_id) const {
		return get<kind::Function>().flatMap([parameter_id](CRef<kind::Function> function) {
			if (parameter_id >= function->parameters.size()) return base::Optional<TypeCRef>();
			return base::Optional<TypeCRef>(function->parameters[parameter_id]);
		});
	}

	base::Optional<TypeCRef> Type::getResultType() const {
		return get<kind::Function>().flatMap([](CRef<kind::Function> function) {
			return base::Optional<TypeCRef>(function->result);
		});
	}
}
