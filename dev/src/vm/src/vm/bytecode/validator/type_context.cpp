#include "type_context.hpp"

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_builder.hpp>
#include <vm/bytecode/validator/type_validator.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

using namespace vm::code;

const vm::ObjIdNameMap<TypeOfData>& TypeContext::getCurrentTypes() const { return types; }

void TypeContext::insertType(const TypeOfData& type) {
	const auto name = typeName(type);
	match_optional(types.atMaybe(name)) {
		opt_some(previous_type) {
			if (type != *previous_type) throw DuplicatedTypeError(type, *previous_type);
		}
		opt_none { types.insert(type, name); }
	}
}
