#include "stack_state.hpp"

#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <vm/program/opcode_args.hpp>

namespace vm::validator {
	void StackState::push(TypeCRef type) {
		// @TODO this is silly, until we figure out what we do with void
		if (type->getSize() == 0) return;
		auto offset = offset_stack.empty() ? 0 : offset_stack.back() + top()->getSize();
		offset_to_type.put(offset, type);
		offset_stack.push_back(offset);
	}

	void StackState::pop() {
        auto offset = offset_stack.back();
        offset_stack.pop_back();
        offset_to_type.erase(offset);
	}

	TypeCRef StackState::top() const { return offset_to_type[offset_stack.back()]; }

	bool StackState::empty() const { return offset_stack.empty(); }

	bool StackState::consume(CRef<parser::OpCode> opcode) {
		if (opcode->opcode_name == "init_type") {
			variant_match(opcode->args.at(0).arg) {
				variant_case(opargs::Type, tp) {
					push(type_metadata.getTypeByName(tp.type_name).expect("Invalid type"));
				}
				variant_default CORE_PANIC("Expected type in arg");
			}
		} else if (opcode->opcode_name == "deinit") {
			if (empty()) return false;
			pop();
		} else if (opcode->opcode_name == "call_func") {
			variant_match(opcode->args[0].arg) {
				variant_case(opargs::FunctionName, function_name) {
					CRef<Type> function_type
						= type_metadata.getTypeByName(function_name.function_name)
					          .expect("Invalid funciton type");
					auto fun = function_type->get<kind::Function>();
					for (auto function_param: fun->parameters | std::views::reverse)
						if (empty() || function_param != top())
							return false;
						else
							pop();
					// @TODO this seems pretty hacky to me, will be gone with builders at least :)
					bool returns_void = fun->result->getSize() == 0;
					if ((empty() || fun->result != top()) && !returns_void) return false;
				}
				variant_default { CORE_PANIC("expected function name after call_func opcode"); }
			}
		} else if (opcode->opcode_name == "ret") {
			// @TODO again, this is silly
			if (return_type->getSize() == 0) return true;
			if_opt_some(atOffset(0), type) { return type == return_type; }
			return false;
		}

		return true;
	}

	base::Optional<TypeCRef> StackState::atOffset(usize offset) const {
		return offset_to_type.atMaybeCopy(offset);
	}

	StackState::StackState(const TypeMetadata& meta_data, TypeCRef function_type):
		  type_metadata(meta_data),
		  return_type(function_type->getResultType().value()) {
		auto fun = function_type->get<kind::Function>();
		push(return_type);
		for (auto param: fun->parameters) push(param);
	}
}
