#pragma once

#include <vm/core/process/type_metadata/type.hpp>
#include <vm/loader/parser/elements.hpp>

#include <vector>

namespace vm::loader::validator {
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
	class StackState final {
		std::vector<CRef<Type>> stack_state;
		const TypeMetadata&     type_metadata;

	public:
		explicit StackState(const TypeMetadata& meta_data);

		/**
		 * @brief Takes an instruction and updates the stack.
		 *
		 * Returns true if the stack everything is fine.
		 * Otherwise, returns false.
		 */
		bool consume(CRef<parser::OpCode> opcode);
	};
}
