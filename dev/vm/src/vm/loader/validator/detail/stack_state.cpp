#include "stack_state.hpp"

#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <vm/code/opcode_args.hpp>

bool vm::loader::validator::StackState::consume(CRef<parser::OpCode> opcode) {
	if (opcode->opcode_name == "init_type") {
		variant_match(opcode->args.at(0)) {
			variant_case(opargs::Type, tp) {
				stack_state.push_back(type_metadata.atMaybe(tp.type_name).expect("Invalid type"));
			}
			variant_default CORE_PANIC("Expected type in arg");
		}
	} else if (opcode->opcode_name == "deinit") {
		if (stack_state.empty()) return false;
		stack_state.pop_back();
	} else if (opcode->opcode_name == "call_func") {
		// TODO
	}
	return true;
}

vm::loader::validator::StackState::StackState(const TypeMetadata& meta_data):
	  type_metadata(meta_data) {}
