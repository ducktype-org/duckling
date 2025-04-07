#pragma once

#include <vm/core/process/type_metadata/type.hpp>
#include <vm/preprocessor/parser/elements.hpp>
#include <vm/preprocessor/parser/type_of_data.hpp>

#include <vector>

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
		std::vector<usize>             offset_stack;
		base::HashMap<usize, TypeCRef> offset_to_type;
		const TypeMetadata&            type_metadata;
		TypeCRef                       return_type;

		void     push(TypeCRef type);
		void     pop();
		TypeCRef top() const;
		bool     empty() const;

	public:
		StackState(const TypeMetadata& meta_data, TypeCRef function_type);

		/**
		 * @brief Takes an instruction and updates the stack.
		 *
		 * Returns true if the stack everything is fine.
		 * Otherwise, returns false.
		 */
		bool consume(CRef<parser::OpCode> opcode);

		[[nodiscard]] base::Optional<TypeCRef> atOffset(usize offset) const;
	};
}
