#pragma once


#include "../../../../../../VM/src/preprocessor/parser/types_of_data.hpp"
// #include <base/string_id.hpp>
#include "base/exceptions.hpp"
#include "instructions.hpp"
#include <vector>
#include "elements.hpp"

namespace compiler::backend_vm {
	template<class T>
	class Builder {
	public:
		virtual ~Builder()                    = default;
		[[nodiscard]] virtual T build() const = 0;
	};

	class BlockBuilder: public Builder<Block> {
		// @TODO: Decide if we need it.

		// Jump instructions are only allowed in context
		// of a single block.

		std::vector<VmInstruction> instructions{};

	public:
		BlockBuilder() = default;

		void addBlock(const BlockBuilder& block);
		void addInstruction(const VmInstruction& instruction);

		[[nodiscard]] Block build() const override;
	};

	/**
	 * @brief Creates a VM function from blocks.
	 */
	class FunctionBuilder: public Builder<Function> {
		usize stack_size    = 0;
		usize arg_size      = 0;
		usize next_arg_size = 0;
		usize ret_size      = 0;

	public:
		FunctionBuilder() = default;

		[[nodiscard]] Function build() const override;
	};

	class FileBuilder: public Builder<CodeFile> {
		std::vector<Function>       functions{};
		std::vector<vm::TypeOfData> types{};

	public:
		FileBuilder() = default;

		void addFunction(const FunctionBuilder& function);

		void addType(const vm::TypeOfData& type);

		[[nodiscard]] CodeFile build() const override;
	};
}
