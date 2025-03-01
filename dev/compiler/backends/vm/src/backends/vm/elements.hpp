#pragma once

#include <ostream>
#include <vector>
#include "../../../../../../VM/src/preprocessor/parser/types_of_data.hpp"
#include "backends/vm/instructions.hpp"

namespace compiler::backend_vm {
	template<class T>
	class Builder {
	public:
		virtual ~Builder()                    = default;
		[[nodiscard]] virtual T build() const = 0;
	};

	struct VmElement {
	public:
		virtual ~VmElement()                            = default;
		virtual void serialize(std::ostream& out) const = 0;
	};

	/**
	 * @brief Represents a block of instructions.
	 */
	class Block: VmElement {
		friend class Builder<Block>;
		friend class Function;
		std::vector<VmInstruction> instructions{};

		Block() = default;

	public:
		void serialize(std::ostream& out) const override;
	};

	/**
	 * @brief Represents bytecode a function.
	 */
	class Function: VmElement {
		friend class Builder<Function>;

		usize stack_size    = 0;
		usize arg_size      = 0;
		usize next_arg_size = 0;
		usize ret_size      = 0;

		Block body;

		Function() = default;

	public:
		void serialize(std::ostream& out) const override;
	};

	/**
	 * @brief Represents a file. File may contain multiple
	 * functions and type definitions.
	 */
	class CodeFile: VmElement {
		friend class Builder<CodeFile>;

		std::vector<vm::TypeOfData> types{};
		std::vector<Function>       functions{};

		CodeFile() = default;

	public:
		void serialize(std::ostream& out) const override;
	};
}
