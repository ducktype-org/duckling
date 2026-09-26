#pragma once

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/valid_function.hpp>

namespace vm::loader {
	struct FatBytecodePosition {
		base::StrID function_name;
		usize       instruction_index;
	};

	enum class MappingException {
		MissingMapping,
		NoFunction,
	};

	class ValidFuncPosition final {
		CRef<code::valid_function::ValidFunction> valid_function;
		[[maybe_unused]] u64                      line;
		code::StackStateID                        state;

	public:
		[[nodiscard]]
		base::Optional<code::valid_type::TypeSize> getByteOffset(base::StrID name) const {
			return valid_function->local_stack.getByteOffset(state, name);
		}

		[[nodiscard]]
		bool contains(base::StrID name) const {
			return valid_function->local_stack.contains(state, name);
		}

		[[nodiscard]]
		base::Optional<usize> getBlockIdx(base::StrID name) const {
			return valid_function->local_stack.getBlockIdx(state, name);
		}

		[[nodiscard]]
		base::Optional<base::StrID> getTypeName(base::StrID name) const {
			return valid_function->local_stack.getTypeName(state, name);
		}

		[[nodiscard]]
		base::Optional<base::StrID> getTypeName(usize idx) const {
			return valid_function->local_stack.getTypeName(state, idx);
		}

		[[nodiscard]]
		base::Optional<base::StrID> getName(usize idx) const {
			return valid_function->local_stack.getName(state, idx);
		}

		[[nodiscard]]
		base::Optional<usize> size() const {
			return valid_function->local_stack.size(state);
		}

		[[nodiscard]]
		std::pair<u64, code::valid_type::TypeSize> getBaseOffset() const {
			return valid_function->local_stack.getBaseOffset();
		}

		[[nodiscard]]
		base::Optional<usize> absoluteSize() const {
			return valid_function->local_stack.absoluteSize(state);
		}

		[[nodiscard]]
		base::Optional<code::valid_type::TypeSize> byteSize() const {
			return valid_function->local_stack.byteSize(state);
		}

		ValidFuncPosition(usize line, CRef<code::valid_function::ValidFunction> valid_function):
			  valid_function(valid_function),
			  line(line) {
			auto& stack_states = valid_function->stack_states;
			CORE_ASSERT(line < stack_states.size(), "line is out of bounds");
			state = stack_states.at(line);
		}
	};
}
