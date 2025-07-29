#include "local_stack.hpp"

#include "errors.hpp"

#include <base/exceptions.hpp>
#include <base/ref.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <variant>

using namespace vm;
using namespace code;
using namespace func_validator_helpers;


constexpr bool LocalStackEntry::operator==(const LocalStackEntry& other) const {
	return local_name == other.local_name && *type == *other.type;
}

const std::vector<LocalStackEntry>& LocalStack::getStackState() const { 
	return stack_state;
}

void LocalStack::push(const opargs::StackLocalAny& local, const opargs::Type& type) {
	auto tod = tod_map->at(type.type_name);

	if (local_name_to_type.contains(local.var_name)) throw DuplicatedLocalNameError(local);

	stack_state.emplace_back(local.var_name, tod);
	local_name_to_type.put(local.var_name, tod);
}

template<DeinitializingInstruction InstructionType>
void LocalStack::pop(const InstructionType& cause) {
	if (stack_state.size() == 1) throw RetValDeinitError(cause);
	const auto& top = stack_state.back();
	local_name_to_type.erase(top.local_name);
	stack_state.pop_back();
}

usize LocalStack::size() const {
	return stack_state.size(); 
}

const LocalStackEntry& LocalStack::back() const {
	return stack_state.back(); 
}

const LocalStackEntry& LocalStack::front() const {
	return stack_state.front(); 
}

void LocalStack::castPrimitive(const opargs::OpCodePrimitiveArg& local, const opargs::Type& type) {
	auto  local_name = VISIT(local, l, return l.var_name);
	auto& curr_type  = local_name_to_type.at(local_name);
	auto  new_type   = tod_map->at(type.type_name);
	curr_type        = new_type;
	for (auto& entry: stack_state)
		if (entry.local_name == local_name) entry.type = new_type;
}

bool LocalStack::contains(base::StrID local_name) const {
	return local_name_to_type.contains(local_name); 
}

CRef<TypeOfData> LocalStack::at(base::StrID local_name) const {
	return local_name_to_type.at(local_name); 
}
