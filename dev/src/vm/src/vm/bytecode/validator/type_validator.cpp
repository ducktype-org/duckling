#include "type_validator.hpp"

#include "type_validator_helpers.hpp"

using namespace vm::code;
using namespace type_validator_helpers;

Box<vm::TypeMetadata> TypeContext::validateAndProduceTypeMetadata() const {
	validateTypes(*this);
	Box<TypeMetadata> metadata = makeBox<TypeMetadata>();

	// Declare all types first
	for (const auto& type: types) metadata->addType(Type::declareType(typeName(type)));

	// Well-define every type.
	for (const auto& type: types) {
		variant_match(type) {
			variant_case(PrimitiveType, data) {
				metadata->at(data.name)->definePrimitive(TypeSize(data.size));
			}
			variant_case(PointerType, data) {
				metadata->at(data.name)->definePointer(
					metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(FixedSizeTableType, data) {
				metadata->at(data.name)->defineFixedSizeTable(
					metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner),
					data.table_size
				);
			}
			variant_case(DynamicTableType, data) {
				metadata->at(data.name)->defineDynamicTable(
					metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(DataType, data) {
				FieldVector fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						metadata->atMaybe(field.type).expect<UnknownSubtypeError>(data, field.name)
					);
				metadata->at(data.name)->defineData(fields, {});
			}
			variant_case(VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(
						metadata->atMaybe(variant).expect<UnknownSubtypeError>(data, variant)
					);
				metadata->at(data.name)->defineVariant(variants);
			}
			variant_case(FunctionType, data) {
				std::vector<vm::TypeCRef> parameters;
				parameters.reserve(data.parameters.size());
				for (auto& param: data.parameters) parameters.emplace_back(metadata->at(param));
				metadata->at(data.name)->defineFunction(
					parameters,
					metadata->atMaybe(data.result).expect<UnknownSubtypeError>(data, data.result)
				);
			}
			variant_case(OpaqueType, opaque) {
				metadata->at(opaque.name)->defineOpaque(TypeSize(opaque.size));
			}
			variant_case(ClassType, clazz) {
				TypeRef                 tp     = metadata->at(clazz.name);
				FieldVector             fields = buildFieldVector(clazz, *metadata, *this);
				vm::InheritanceMetadata inh_metadata
					= buildInheritanceMetadata(clazz, *metadata, *this);
				tp->defineData(fields, std::move(inh_metadata));
			}
			variant_case(InterfaceType, interface) {
				TypeRef                 tp     = metadata->at(interface.name);
				FieldVector             fields = buildFieldVector(interface, *metadata, *this);
				vm::InheritanceMetadata inh_metadata
					= buildInheritanceMetadata(interface, *metadata, *this);
				tp->defineData(fields, std::move(inh_metadata));
			}
			variant_default { CORE_PANIC("bad type"); }
		}
	}
	metadata->finalize();
	return metadata;
}

const vm::StableObjIdNameMap<TypeOfData>& TypeContext::getCurrentTypes() const { return types; }

void TypeContext::insertType(const TypeOfData& type) {
	const auto name = typeName(type);
	match_optional(types.atMaybe(name)) {
		opt_some(previous_type) {
			if (type != *previous_type) throw DuplicatedTypeError(type, *previous_type);
		}
		opt_none { types.insert(type, name); }
	}
}
