#include "type.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>

#include <algorithm>

namespace vm {
	using base::Optional;

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
	void Type::definePrimitive(TypeSize pass_size, ShadowSize pass_shadow_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type   = Kind::Primitive;
		size        = pass_size;
		shadow_size = pass_shadow_size;
		kind        = kind::Primitive();
		if (name == "void") am_i_instantiable = false;
	}

	void Type::definePointer(TypeCRef inner, ShadowSize pass_shadow_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		size        = POINTER_SIZE;
		shadow_size = pass_shadow_size;
		kind_type   = Kind::Pointer;
		kind        = kind::Pointer{ inner };
	}

	void Type::defineFixedSizeTable(TypeRef inner, u64 element_count, ShadowSize pass_shadow_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type   = Kind::FixedSizeTable;
		shadow_size = pass_shadow_size;
		kind        = kind::FixedSizeTable{ .inner_type = inner, .element_count = element_count };
	}

	void Type::defineDynamicTable(TypeRef inner, ShadowSize pass_shadow_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type         = Kind::DynamicTable;
		shadow_size       = pass_shadow_size;
		kind              = kind::DynamicTable{ .inner_type = inner };
		am_i_instantiable = false;
	}

	void Type::defineData(
		const std::vector<FieldDefinition>&  fields_definitions,
		base::Optional<InheritanceMetadata>  inheritance_metadata,
		ShadowSize                           pass_shadow_size
	) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type   = Kind::Data;
		shadow_size = pass_shadow_size;
		auto data   = kind::Data{};
		for (const auto& field_def: fields_definitions) {
			data.field_name_map.put(field_def.name, data.fields.size());
			// byte offset is set during finalization; shadow offset comes from ValidType
			data.fields.emplace_back(kind::FieldDesc{ .offset        = Offset(0),
				.shadow_offset = field_def.shadow_offset,
				.shadow_size   = field_def.type->getShadowSize(),
				.type          = field_def.type });
		}
		data.inheritance_metadata = std::move(inheritance_metadata);
		kind                      = data;
	}

	void Type::defineVariant(Bytes type_tag_size, const std::vector<TypeRef>& variants_definitions, ShadowSize pass_shadow_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		CORE_ASSERT(variants_definitions.size() != 0, "Cannot define variant with no alternatives");
		state = State::Defined;

		kind_type   = Kind::Variant;
		shadow_size = pass_shadow_size;
		auto variant = kind::Variant{};
		for (const auto& type: variants_definitions) variant.alternatives.push_back(type);

		variant.type_tag_size = type_tag_size;
		kind                  = variant;
	}

	void Type::defineFunction(std::vector<TypeCRef> parameters, std::vector<TypeCRef> result, ShadowSize pass_shadow_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		size        = POINTER_SIZE;
		shadow_size = pass_shadow_size;
		kind_type   = Kind::Function;
		kind        = kind::Function{ .parameters   = std::move(parameters),
			                       .result_types = std::move(result) };
	}

	void Type::defineOpaque(TypeSize pass_size, ShadowSize pass_shadow_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type   = Kind::Opaque;
		size        = pass_size;
		shadow_size = pass_shadow_size;
		kind        = kind::Opaque{};
	}

	void Type::finalize() {
		// @TODO: #1971 Delete these checks
		if (state == State::Finalizing)
			CORE_PANIC("Cyclic dependency detected during type finalization");
		if (state == State::Finalized) return;
		state = State::Finalizing;
		defer(state = State::Finalized);

		variant_match(kind) {
			variant_case(kind::FixedSizeTable, fixed_size_table) {
				fixed_size_table.inner_type->finalize();
				this->size = fixed_size_table.inner_type->getSize() * fixed_size_table.element_count;
				// shadow_size is pre-set from ValidType via defineFixedSizeTable
			}
			variant_case(kind::Data, data) {
				// calculate byte offsets only; shadow offsets are pre-set from ValidType
				Offset offset(0);
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
				// calculate byte size only; shadow size is pre-set from ValidType
				TypeSize data_size(0);
				for (auto& alternative: variant.alternatives) {
					alternative->finalize();
					data_size = std::max(data_size, alternative->getSize());
				}
				this->size = variant.type_tag_size + data_size;
				isInstantiableImpl(variant);
			}
			variant_default {}
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

    base::Optional<ShadowOffset> Type::getFieldShadowOffsetByName(base::StrID field_name) const {
		return get<kind::Data>().flatMap(
			[field_name](CRef<kind::Data> data) -> base::Optional<ShadowOffset> {
				if_opt_some(data->field_name_map.atMaybe(field_name), field_index) {
					return data->fields[*field_index].shadow_offset;
				}
				return {};
			}
		);

    }

	base::Optional<CRef<std::vector<kind::FieldDesc>>> Type::getFields() const {
		return get<kind::Data>().map([](CRef<kind::Data> data) { return CRef(&data->fields); });
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

	base::Optional<Bytes> Type::getParametersSize() const {
		return get<kind::Function>().map([](CRef<kind::Function> function) {
			Bytes size(0);
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

	base::Optional<u64> Type::getResultTypeCount() const {
		return get<kind::Function>().map([](CRef<kind::Function> function) {
			return function->result_types.size();
		});
	}

	base::Optional<Bytes> Type::getResultTypeSize() const {
		return get<kind::Function>().map([](CRef<kind::Function> function) {
			Bytes size(0);
			for (const auto& reslt: function->result_types) size += reslt->getSize();
			return size;
		});
	}

	base::Optional<TypeCRef> Type::getNthResultType(u64 parameter_id) const {
		return get<kind::Function>().flatMap(
			[parameter_id](CRef<kind::Function> function) -> base::Optional<TypeCRef> {
				if (parameter_id >= function->result_types.size()) return std::nullopt;
				return { function->result_types[parameter_id] };
			}
		);
	}

	base::Optional<Bytes> Type::getTypeTagSizeBytes() const {
		return get<kind::Variant>().map([](CRef<kind::Variant> variant) {
			return variant->type_tag_size;
		});
	}

	base::Optional<std::vector<TypeCRef>> Type::getVariantAlternatives() const {
		return get<kind::Variant>().map([](CRef<kind::Variant> variant) {
			std::vector<TypeCRef> alternatives;
			alternatives.reserve(variant->alternatives.size());
			for (const auto& alt: variant->alternatives) alternatives.emplace_back(alt);
			return alternatives;
		});
	}

	bool Type::isTriviallyCopyable() const {
		variant_match(kind) {
			variant_case(kind::Data, data) {
				for (const auto& field: data.fields)
					if (!field.type->isTriviallyCopyable()) return false;
				return true;
			}
			variant_case(kind::DynamicTable, table) { return false; }
			variant_case(kind::FixedSizeTable, table) {
				return table.inner_type->isTriviallyCopyable();
			}
			variant_case(kind::Variant, variant) { return false; }
			variant_case(kind::Function, function) { return false; }
			variant_case(kind::Pointer, pointer) { return false; }
			variant_case(kind::Opaque, opaque) { return true; }
			variant_case(kind::Primitive, primitive) { return true; }
			variant_default { CORE_PANIC("This should never happen"); }
		}
		CORE_UNREACHABLE();
	}
	u32 Type::getShadowEntryIndex(u64 byte_offset) const {
		variant_match(kind) {
			variant_case(kind::Data, data) {
				if (byte_offset < data.byte_to_shadow.size())
					return data.byte_to_shadow[byte_offset];
				return 0;
			}
			variant_case(kind::FixedSizeTable, table) {
				auto inner = table.inner_type;
				u64 inner_bytes  = inner->getSize().asInt();
				u64 inner_shadow = inner->getShadowSize();
				if (inner_bytes == 0) return 0;
				u64 idx = byte_offset / inner_bytes;
				u64 rem = byte_offset % inner_bytes;
				return static_cast<u32>(idx * inner_shadow + inner->getShadowEntryIndex(rem));
			}
			variant_case(kind::DynamicTable, table) {
				auto inner = table.inner_type;
				u64 inner_bytes  = inner->getSize().asInt();
				u64 inner_shadow = inner->getShadowSize();
				if (inner_bytes == 0) return 0;
				u64 idx = byte_offset / inner_bytes;
				u64 rem = byte_offset % inner_bytes;
				return static_cast<u32>(idx * inner_shadow + inner->getShadowEntryIndex(rem));
			}
			variant_default {
				return 0;
			}
		}
		CORE_UNREACHABLE();
	}

	void Type::setByteToShadow(std::vector<u32> b2s) {
		variant_match(kind) {
			variant_case(kind::Data, data) {
				data.byte_to_shadow = std::move(b2s);
			}
			variant_default {}
		}
	}


}
