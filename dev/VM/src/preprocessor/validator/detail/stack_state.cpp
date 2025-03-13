#include "stack_state.hpp"
#include "base/exceptions.hpp"
#include "base/variant.hpp"
#include "code_data/opcode_args.hpp"

void vm::validator::StackState::consume(CRef<parser::OpCode> opcode) {
	if (opcode->opcode_name == "init_type") {
		variant_match(opcode->args.at(0).arg) {
			variant_case(opargs::Type, tp) {
				stack_state.push_back(
					type_metadata.getTypeByName(tp.type_name).expect("Invalid type")
				);
			}
			variant_default CORE_PANIC("Expected type in arg");
		}
	} else if (opcode->opcode_name == "deinit") {
		CORE_ASSERT(!stack_state.empty(), "Cannot deinit empty stack");
		stack_state.pop_back();
	} else if (opcode->opcode_name == "call_func") {
		// TODO
	}
}

vm::validator::StackState::StackState(const TypeMetadata& meta_data): type_metadata(meta_data) {}
