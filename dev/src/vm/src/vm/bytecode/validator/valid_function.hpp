#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/local_stack_database.hpp>

namespace vm::code::valid_function {
	struct ValidFunction final: ElementBase {
		Identifier                name;
		CodeBlock                 body;
		std::vector<StackStateID> stack_states;
		FuncSignature             signature;
		LocalStackDb              local_stack;

		/**
		 * @brief constructs a normal (not validated) function, which from a valid function
		 * @warning THIS FUNCTION IS O(n) - USE IT CAREFULY.
		 */
		Function toNormal() const {
			Function new_func;

			new_func.bytecode_pos = bytecode_pos;
			new_func.name         = name;
			new_func.body         = body;
			new_func.signature    = signature;

			return new_func;
		}

		base::Optional<valid_type::TypeSize> getByteOffset(u64 line, base::StrID name) const {
			if (line >= stack_states.size()) return std::nullopt;
			auto state = stack_states.at(line);
			return local_stack.getByteOffset(state, name);
		}

		base::Optional<bool> contains(u64 line, base::StrID name) const {
			if (line >= stack_states.size()) return std::nullopt;
			auto state = stack_states.at(line);
			return local_stack.contains(state, name);
		}

		base::Optional<usize> getIdx(u64 line, base::StrID name) const {
			if (line >= stack_states.size()) return std::nullopt;
			auto state = stack_states.at(line);
			return local_stack.getIdx(state, name);
		}

		base::Optional<base::StrID> getTypeName(u64 line, base::StrID name) const {
			if (line >= stack_states.size()) return std::nullopt;
			auto state = stack_states.at(line);
			return local_stack.getTypeName(state, name);
		}

		base::Optional<base::StrID> getTypeName(u64 line, usize idx) const {
			if (line >= stack_states.size()) return std::nullopt;
			auto state = stack_states.at(line);
			return local_stack.getTypeName(state, idx);
		}

		base::Optional<base::StrID> getName(u64 line, usize idx) const {
			if (line >= stack_states.size()) return std::nullopt;
			auto state = stack_states.at(line);
			return local_stack.getName(state, idx);
		}

		base::Optional<usize> size(u64 line) const {
			if (line >= stack_states.size()) return std::nullopt;
			auto state = stack_states.at(line);
			return local_stack.size(state);
		}

		base::Optional<valid_type::TypeSize> byteSize(u64 line) const {
			if (line >= stack_states.size()) return std::nullopt;
			auto state = stack_states.at(line);
			return local_stack.byteSize(state);
		}

		[[nodiscard]]
		base::Optional<bool> eqTypes(u64 line_1, u64 line_2) const {
			if (line_1 >= stack_states.size()) return std::nullopt;
			if (line_2 >= stack_states.size()) return std::nullopt;
			auto state_1 = stack_states.at(line_1);
			auto state_2 = stack_states.at(line_2);
			return local_stack.eqTypes(state_1, state_2);
		}

		[[nodiscard]]
		base::Optional<bool> eqNames(u64 line_1, u64 line_2) const {
			if (line_1 >= stack_states.size()) return std::nullopt;
			if (line_2 >= stack_states.size()) return std::nullopt;
			auto state_1 = stack_states.at(line_1);
			auto state_2 = stack_states.at(line_2);
			return local_stack.eqNames(state_1, state_2);
		}
	};
}

namespace vm::code {
	using FunctionMap = ObjIdNameMap<valid_function::ValidFunction>;
}
