#include "type.hpp"

#include <bits/ranges_algo.h>

#include <base/defer.hpp>
#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <vm/core/supervisor/supervisor.hpp>

#include <algorithm>
#include <utility>

namespace vm {
	using std::plus;

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

	void Type::defineStaticTable(TypeRef inner, u64 table_size) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type = Kind::StaticTable;
		kind      = kind::StaticTable{ .inner_type = inner, .size = table_size };
	}

	void Type::defineDynamicTable(TypeRef inner) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		size      = POINTER_SIZE;
		kind_type = Kind::DynamicTable;
		kind      = kind::DynamicTable{ inner };
	}

	void Type::defineData(const std::vector<std::pair<base::StrID, TypeRef>>& fields_definitions) {
		CORE_ASSERT(state == State::Declared, "Bad type define");
		state = State::Defined;

		kind_type = Kind::Data;
		auto data = kind::Data{};
		for (auto [sub_name, sub_type]: fields_definitions) {
			data.field_name_map[sub_name] = data.fields.size();
			// offset is set during finalization
			data.fields.emplace_back(kind::FieldDesc{ .offset = 0, .type = sub_type });
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

		size      = POINTER_SIZE;
		kind_type = Kind::Function;
		kind      = kind::Function{ .parameters = std::move(parameters), .result = result };
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
		CORE_UNREACHABLE();
	}

	// pointer, staticTable, dynamicTable
	base::Optional<TypeCRef> Type::getInnerType() const {
		auto get_inner_type = [](const auto& t) { return t.inner_type; };

		auto pointer_option = get<kind::Pointer>().map(get_inner_type);
		if (pointer_option.has_value()) return (TypeCRef) pointer_option.value();

		auto static_table_option = get<kind::StaticTable>().map(get_inner_type);
		if (static_table_option.has_value()) return (TypeCRef) static_table_option.value();

		auto dynamic_table_option = get<kind::DynamicTable>().map(get_inner_type);
		if (dynamic_table_option.has_value()) return (TypeCRef) dynamic_table_option.value();

		return {};
	}

	// staticTable
	base::Optional<u64> Type::getStaticTableSize() const {
		return get<kind::StaticTable>().map([](const kind::StaticTable& table) {
			return table.size;
		});
	}

	// struct
	base::Optional<TypeCRef> Type::getFieldType(kind::Data::FieldID field_id) const {
		return get<kind::Data>().flatMap([field_id](const kind::Data& data) {
			if (field_id >= data.fields.size()) return base::Optional<TypeCRef>();
			return base::Optional<TypeCRef>(data.fields[field_id].type);
		});
	}

	base::Optional<Offset> Type::getFieldOffset(kind::Data::FieldID field_id) const {
		return get<kind::Data>().flatMap([field_id](const kind::Data& data) {
			if (field_id >= data.fields.size()) return base::Optional<Offset>();
			return base::Optional<Offset>(Offset(data.fields[field_id].offset));
		});
	}

	base::Optional<TypeCRef> Type::getFieldTypeByOffset([[maybe_unused]] Offset offset) const {
		throw base::NotYetImplemented("getFieldTypeByOffset");
		// return get<kind::Data>().flatMap([offset](const kind::Data& data) {
		// 	// @TODO: Verify this code
		// 	usize begin = 0, end = data.fields.size(), middle = 0;
		// 	while (end - begin) {
		// 		middle = (begin + end) / 2;
		// 		if (data.fields[middle].offset < offset)
		// 			begin = middle + 1;
		// 		else
		// 			end = middle;
		// 	}
		// 	if (data.fields[begin].offset != offset) return base::Optional<TypeCRef>();
		// 	return base::Optional<TypeCRef>(data.fields[begin].type);
		// });
	}

	base::Optional<TypeCRef> Type::getFieldTypeByOffsetRecursive([[maybe_unused]] Offset offset
	) const {
		throw base::NotYetImplemented("getFieldTypeByOffsetRecursive");
		// return get<kind::Data>().flatMap([offset](const kind::Data& data) {
		// 	// @TODO: Verify this code
		// 	usize begin = 0, end = usize(data.fields.size()), middle = 0;
		// 	while (end - begin) {
		// 		middle = (begin + end) / 2;
		// 		if (data.fields[middle].offset < offset)
		// 			begin = middle + 1;
		// 		else
		// 			end = middle;
		// 	}
		// 	if (data.fields[begin].offset != offset)
		// 		return data.fields[begin].type->getFieldTypeByOffsetRecursive(
		// 			offset - data.fields[begin].offset
		// 		);
		// 	return base::Optional<TypeCRef>(data.fields[begin].type);
		// });
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

	base::Optional<u64> Type::getParametersSize() const {
		return get<kind::Function>().map([](const kind::Function& function) {
			usize size = 0;
			for (const auto& param: function.parameters) size += param->getSize();
			return size;
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

	base::Optional<const std::vector<TypeCRef>&> Type::getParameters() const {
		return get<kind::Function>().map(
			[](const kind::Function& func) -> const std::vector<TypeCRef>& {
				return func.parameters;
			}
		);
	}
}
