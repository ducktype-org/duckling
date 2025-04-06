#include "stack_state.hpp"

#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <vm/program/opcode_args.hpp>

bool vm::validator::StackState::consume(CRef<parser::OpCode> opcode) {
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
		if (stack_state.empty()) return false;
		stack_state.pop_back();
	} else if (opcode->opcode_name == "call_func") {
		variant_match(opcode->args[0].arg) {
			variant_case(opargs::FunctionName, function_name) {
				CRef<Type> function_type = type_metadata.getTypeByName(function_name.function_name)
				                               .expect("Invalid funciton type");
				auto fun = function_type->get<kind::Function>();
				for (auto function_param: fun->parameters)
					if (stack_state.empty() || function_param != stack_state.back())
						return false;
					else
						stack_state.pop_back();
				if (stack_state.empty() || fun->result != stack_state.back()) return false;
			}
			variant_default { CORE_PANIC("expected function name after call_func opcode"); }
		}
	}
	return true;
}

vm::validator::StackState::StackState(const TypeMetadata& meta_data): type_metadata(meta_data) {}
