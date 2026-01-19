#include "type_context.hpp"

#include <vm/bytecode/validator/errors.hpp>

using namespace vm::code;

const vm::ObjIdNameMap<TypeOfData>& TypeContext::getCurrentTypes() const { return pod_types; }

void TypeContext::insertType(const TypeOfData& type) {
	const auto name = typeName(type);
	match_optional(pod_types.atMaybe(name)) {
		opt_some(previous_type) {
			if (type != *previous_type) throw DuplicatedTypeError(type, *previous_type);
		}
		opt_none { pod_types.insert(type, name); }
	}
}
