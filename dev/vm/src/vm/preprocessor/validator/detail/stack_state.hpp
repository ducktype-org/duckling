#pragma once

#include "vm/core/process/type_metadata/type.hpp"
#include "vm/preprocessor/parser/elements.hpp"
#include <vector>
#include "vm/preprocessor/parser/type_data.hpp"

namespace vm::validator {
	/**
	 * @brief Holds a state of the local stack.
	 *
	 * It also performs a basic check for init and deinit instructions.
	 * It checks if:
	 * - init_type got a valid type as an argument
	 * - deinit is not called on an empty stack
	 *
	 * @todo Add tests.
	 */
	class StackState {
		std::vector<CRef<Type>> stack_state;
		const TypeMetadata&     type_metadata;

	public:
		StackState(const TypeMetadata& meta_data);

		void consume(CRef<parser::OpCode> opcode);
	};
}
