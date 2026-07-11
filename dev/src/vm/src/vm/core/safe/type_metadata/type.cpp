#include "type.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>

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

	void Type::defineCPointer(base::Optional<TypeCRef> inner, ShadowSize pass_shadow_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		// A raw native address, not a fat VM pointer - always 8 bytes.
		size        = TypeSize(Bytes(8));
		shadow_size = pass_shadow_size;
		kind_type   = Kind::CPointer;
		kind        = kind::CPointer{ .inner_type = inner };
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
		const std::vector<std::tuple<base::StrID, TypeRef, Offset, ShadowOffset>>& fields_definitions,
		const TypeSize                      data_size,
		base::Optional<InheritanceMetadata> inheritance_metadata,
		ShadowSize                          pass_shadow_size
	) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type   = Kind::Data;
		size        = data_size;
		shadow_size = pass_shadow_size;
		auto data   = kind::Data{};
		for (auto [sub_name, sub_type, sub_offset, sub_shadow_offset]: fields_definitions) {
			data.field_name_map.put(sub_name, data.fields.size());
			data.fields.emplace_back(kind::FieldDesc{
				.offset = sub_offset, .shadow_offset = sub_shadow_offset, .type = sub_type });
		}
		data.inheritance_metadata = std::move(inheritance_metadata);
		kind                      = data;
	}

	void Type::defineVariant(
		Bytes                       type_tag_size,
		const std::vector<TypeRef>& variants_definitions,
		ShadowSize                  pass_shadow_size
	) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		CORE_ASSERT(variants_definitions.size() != 0, "Cannot define variant with no alternatives");
		state = State::Defined;

		kind_type    = Kind::Variant;
		shadow_size  = pass_shadow_size;
		auto variant = kind::Variant{};
		for (const auto& type: variants_definitions) variant.alternatives.push_back(type);

		variant.type_tag_size = type_tag_size;
		kind                  = variant;
	}

	void Type::defineFunction(
		std::vector<TypeCRef> parameters, std::vector<TypeCRef> result, ShadowSize pass_shadow_size
	) {
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

		// Whether a type has references to release is decided by the validator, which is the
		// source of truth for it, and read from there by the lowering.
		variant_match(kind) {
			variant_case(kind::FixedSizeTable, fixed_size_table) {
				fixed_size_table.inner_type->finalize();
				this->size
					= fixed_size_table.inner_type->getSize() * fixed_size_table.element_count;
			}
			variant_case(kind::Data, data) {
				for (auto& field: data.fields) field.type->finalize();
				if_opt_some(data.inheritance_metadata, imd) { inheritsFromImpl(imd); }
				isInstantiableImpl(data);
				buildByteToShadow(data);
			}
			variant_case(kind::Variant, variant) {
				// calculate size
				TypeSize data_size(0);
				for (auto& alternative: variant.alternatives) {
					alternative->finalize();
					data_size = std::max(data_size, alternative->getSize());
				}
				this->size = variant.type_tag_size + data_size;
				isInstantiableImpl(variant);
			}
		}
	}

	void Type::buildByteToShadow(kind::Data& data) {
		data.byte_to_shadow.assign(size.asInt(), NO_SHADOW_ENTRY);
		for (const auto& field: data.fields) {
			const usize field_begin = field.offset.asInt();
			const usize field_size  = field.type->getSize().asInt();
			for (usize byte = 0; byte < field_size; ++byte)
				if_opt_some(field.type->shadowEntryIndexOrPadding(byte), entry) {
					data.byte_to_shadow[field_begin + byte] = field.shadow_offset + entry;
				}
		}
	}

	namespace {
		base::Optional<ShadowOffset> tableShadowEntryIndex(TypeCRef element_type, u64 byte_offset) {
			const u64 element_bytes = element_type->getSize().asInt();
			CORE_ASSERT(
				element_bytes != 0, "A table of zero-sized elements has no addressable byte"
			);
			const u64 element = byte_offset / element_bytes;
			return element_type->shadowEntryIndexOrPadding(byte_offset % element_bytes)
			    .map([&](ShadowOffset entry) {
					return base::safeIntConv<ShadowOffset>(
						element * element_type->getShadowSize() + entry
					);
				});
		}
	}

	ShadowOffset Type::getShadowEntryIndex(u64 byte_offset) const {
		return shadowEntryIndexOrPadding(byte_offset)
		    .expect("Byte offset points at padding of a data type");
	}

	base::Optional<ShadowOffset> Type::shadowEntryIndexOrPadding(u64 byte_offset) const {
		switch (kind_type) {
		case Kind::Primitive:
		case Kind::Pointer:
		case Kind::CPointer:
		case Kind::Function:
		case Kind::Opaque:
			CORE_ASSERT(byte_offset < size.asInt(), "Byte offset outside of the object");
			return 0;
		case Kind::Data: {
			const auto& byte_to_shadow = get<kind::Data>().value()->byte_to_shadow;
			CORE_ASSERT(byte_offset < byte_to_shadow.size(), "Byte offset outside of the object");
			const ShadowOffset entry = byte_to_shadow[byte_offset];
			if (entry == NO_SHADOW_ENTRY) return {};
			return entry;
		}
		case Kind::FixedSizeTable:
			CORE_ASSERT(byte_offset < size.asInt(), "Byte offset outside of the object");
			return tableShadowEntryIndex(getInnerType().value(), byte_offset);
		case Kind::DynamicTable:
			return tableShadowEntryIndex(getInnerType().value(), byte_offset);
		case Kind::Variant:
			CORE_ASSERT(byte_offset < size.asInt(), "Byte offset outside of the object");
			return byte_offset < getTypeTagSizeBytes().value().asInt() ? 0U : 1U;
		case Kind::None:
			break;
		}
		CORE_PANIC("Shadow layout asked of an undefined type");
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

	base::Optional<TypeCRef> Type::getFieldTypeByName(base::StrID field_name) const {
		return get<kind::Data>().flatMap(
			[field_name](CRef<kind::Data> data) -> base::Optional<TypeCRef> {
				if_opt_some(data->field_name_map.atMaybe(field_name), field_index) {
					return data->fields[*field_index].type;
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
			variant_case(kind::CPointer, cpointer) { return true; }
			variant_case(kind::Opaque, opaque) { return true; }
			variant_case(kind::Primitive, primitive) { return true; }
			variant_default { CORE_PANIC("This should never happen"); }
		}
		CORE_UNREACHABLE();
	}
}
