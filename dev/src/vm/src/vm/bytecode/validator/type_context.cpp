#include "type_context.hpp"

#include "vm/bytecode/validator/type.hpp"
#include <vm/bytecode/validator/errors.hpp>

using namespace vm::code;

namespace {
	type::Type translateNewType(const TypeMap& types, const TypeOfData& type_of_data) {
		auto name = typeName(type_of_data);
		auto id = types.size();
		type::Type new_type = vm::Type(name)
	}
}

void vm::code::TypeContext::insertAndValidate(
	const std::vector<TypeOfData>&                   new_types,
	const base::HashMap<base::StrID, FuncSignature>& function_signatures
) {

	// Simple check for duplicates and forward declarations.
	for (const auto& type: new_types) {
		const auto name = typeName(type);
		if (pod_types.contains(name)) {
			if (type != *pod_types.at(name))
				throw DuplicatedTypeError(type, *pod_types.at(name));
		} else {
			pod_types.insert(type, name);
		
		}
	}

	for (const auto& type: new_types) {

			types.insert(translateNewType(types, type), name);
	}
}

const TypeMap& TypeContext::getCurrentTypes() const { return types; }

// void TypeContext::insertType(const TypeOfData& type) {
// 	const auto name = typeName(type);
// 	match_optional(pod_types.atMaybe(name)) {
// 		opt_some(previous_type) {
// 			if (type != *previous_type) throw DuplicatedTypeError(type, *previous_type);
// 		}
// 		opt_none { pod_types.insert(type, name); }
// 	}
// }
const vm::ObjIdNameMap<TypeOfData>& vm::code::TypeContext::getPodTypes() const { return pod_types; }
