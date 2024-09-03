#include "type.hpp"
#include "type_metadata.hpp"
#include <supervisor/supervisor.hpp>
#include <base/variant.hpp>
#include <base/defer.hpp>
#include <utility>

namespace vm {
	// Type declaration:
	Type Type::declareType(base::StrId name) {
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

		size      = PointerSize;
		kind_type = Kind::Pointer;
		kind      = kind::Pointer{ std::move(inner) };
	}

	void Type::defineStaticTable(TypeRef inner, u64 table_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type = Kind::StaticTable;
		kind      = kind::StaticTable{ std::move(inner), table_size };
	}

	void Type::defineDynamicTable(TypeRef inner) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		size      = PointerSize;
		kind_type = Kind::DynamicTable;
		kind      = kind::DynamicTable{ std::move(inner) };
	}

	void Type::defineData(const std::vector<std::pair<base::StrId, TypeRef>>& fields_definitions) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type = Kind::Data;
		auto data = kind::Data{};
		for (auto [sub_name, sub_type]: fields_definitions) {
			data.field_name_map[sub_name] = data.fields.size();
			// offset is set during finalization
			data.fields.emplace_back(kind::FieldDesc{ 0, sub_type });
		}
		kind = data;
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

		size      = PointerSize;
		kind_type = Kind::Function;
		kind      = kind::Function{ std::move(parameters), std::move(result) };
	}

	void Type::finalize() {
		if (state == State::Finalizing) {
			// @TODO: better errors
			CORE_PANIC("Cyclic type dependency");
		}
		if (state == State::Finalized) return;
		state = State::Finalizing;
		defer(state = State::Finalized);

		variant_match(kind) {
			variant_case(kind::StaticTable, static_table) {
				static_table.inner_type->finalize();
				this->size = static_table.inner_type->getSize() * static_table.size;
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
			}
			variant_case(kind::Variant, variant) {
				// calculate size
				TypeSize data_size = 0;
				for (auto& alternative: variant.alternatives) {
					alternative->finalize();
					data_size = std::max(data_size, alternative->getSize());
				}
				this->size = 16 + data_size;
			}
		}
	}

	/**
	 * @brief Returns lowest (smallest) type at given position
	 * inside the type.
	 * See: https://github.com/ducktype-org/rift-poc-zpp1/issues/91
	 */
	base::Optional<TypeCRef> Type::getLowestTypeAtPos(Offset pos) const {
		variant_match(kind) {
			variant_case_novalue(kind::Primitive) {
				if (pos == 0)
					return TypeCRef(this);
				else
					return {};
			}
			variant_case_novalue(kind::Pointer) {
				if (pos == 0)
					return TypeCRef(this);
				else
					return {};
			}
			variant_case(kind::StaticTable, static_table) {
				if (pos >= getSize())
					return {};
				else {
					auto inner_size = static_table.inner_type->getSize();
					return static_table.inner_type->getLowestTypeAtPos(pos % inner_size);
				}
			}
			variant_case_novalue(kind::DynamicTable) {
				if (pos == 0)
					return TypeCRef(this);
				else
					return {};
			}
			variant_case(kind::Data, data) {
				throw base::NotYetImplemented("getLowestTypeAtPos data");
			}
			variant_case_novalue(kind::Variant) {
				// @TODO: is pos == 0 then return some special TypeRef to variant index
				// @TODO: is pos == 1 then return error
				if (pos == 2)
					return TypeCRef(this);
				else
					return {};
			}
			variant_default { CORE_PANIC("Unexpected Type kind"); }
		}
		CORE_PANIC("something went wrong");
	}

	// pointer, staticTable, dynamicTable
	base::Optional<TypeCRef> Type::getInnerType() const {
		auto getInnerType = [](const auto& t) { return t.inner_type; };

		auto pointerOption = get<kind::Pointer>().map(getInnerType);
		if (pointerOption.has_value()) return (TypeCRef) pointerOption.value();

		auto staticTableOption = get<kind::StaticTable>().map(getInnerType);
		if (staticTableOption.has_value()) return (TypeCRef) staticTableOption.value();

		auto dynamicTableOption = get<kind::DynamicTable>().map(getInnerType);
		if (dynamicTableOption.has_value()) return (TypeCRef) dynamicTableOption.value();

		return {};
	}

	// staticTable
	base::Optional<u64> Type::getStaticTableSize() const {
		return get<kind::StaticTable>().map([](const kind::StaticTable& table) {
			return table.size;
		});
	}

	// struct
	base::Optional<TypeCRef> Type::getFieldType(kind::Data::FieldId field_id) const {
		return get<kind::Data>().flatMap([field_id](const kind::Data& data) {
			if (field_id >= data.fields.size()) return base::Optional<TypeCRef>();
			return base::Optional<TypeCRef>(data.fields[field_id].type);
		});
	}

	base::Optional<Offset> Type::getFieldOffset(kind::Data::FieldId field_id) const {
		return get<kind::Data>().flatMap([field_id](const kind::Data& data) {
			if (field_id >= data.fields.size()) return base::Optional<Offset>();
			return base::Optional<Offset>(Offset(data.fields[field_id].offset));
		});
	}

	base::Optional<TypeCRef> Type::getFieldTypeByOffset(Offset offset) const {
		return get<kind::Data>().flatMap([offset](const kind::Data& data) {
			i64 begin = -1, end = i64(data.fields.size()), middle = 0;
			while (end - begin > 1) {
				middle = (begin + end) / 2;
				if (data.fields[middle].offset <= offset)
					middle = begin;
				else
					middle = end;
			}
			if (data.fields[begin].offset != offset) return base::Optional<TypeCRef>();
			return base::Optional<TypeCRef>(data.fields[begin].type);
		});
	}

	base::Optional<TypeCRef> Type::getFieldTypeByOffsetRecursive(Offset offset) const {
		return get<kind::Data>().flatMap([offset](const kind::Data& data) {
			i64 begin = -1, end = i64(data.fields.size()), middle = 0;
			while (end - begin > 1) {
				middle = (begin + end) / 2;
				if (data.fields[middle].offset <= offset)
					middle = begin;
				else
					middle = end;
			}
			if (data.fields[begin].offset != offset)
				return data.fields[begin].type->getFieldTypeByOffsetRecursive(
					offset - data.fields[begin].offset
				);
			return base::Optional<TypeCRef>(data.fields[begin].type);
		});
	}

	// variant
	base::Optional<u64> Type::getVariantCount() const {
		return get<kind::Variant>().map([](const kind::Variant& variant) {
			return variant.alternatives.size();
		});
	}

	base::Optional<TypeCRef> Type::getNthVariantType(u64 variant_id) const {
		return get<kind::Variant>().flatMap([variant_id](const kind::Variant& variant) {
			if (variant_id >= variant.alternatives.size()) return base::Optional<TypeCRef>();
			return base::Optional<TypeCRef>(variant.alternatives[variant_id]);
		});
	}

	// function
	base::Optional<u64> Type::getParameterCount() const {
		return get<kind::Function>().map([](const kind::Function& function) {
			return function.parameters.size();
		});
	}

	base::Optional<TypeCRef> Type::getNthParameterType(u64 parameter_id) const {
		return get<kind::Function>().flatMap([parameter_id](const kind::Function& function) {
			if (parameter_id >= function.parameters.size()) return base::Optional<TypeCRef>();
			return base::Optional<TypeCRef>(function.parameters[parameter_id]);
		});
	}

	base::Optional<TypeCRef> Type::getResultType() const {
		return get<kind::Function>().flatMap([](const kind::Function& function) {
			return base::Optional<TypeCRef>(function.result);
		});
	}
}
