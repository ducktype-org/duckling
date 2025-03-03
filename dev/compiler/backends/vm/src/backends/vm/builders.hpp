#pragma once

#include <base/ref.hpp>
#include "../../../../../../VM/src/preprocessor/parser/types_of_data.hpp"
#include "instructions.hpp"
#include <deque>
#include <vector>
#include "elements.hpp"

namespace compiler::backend_vm {

	class BlockBuilder {
		// @TODO: Decide if we need it.

		// Jump instructions are only allowed in context
		// of a single block.

		std::deque<VmInstruction> instructions{};

	public:
		virtual ~BlockBuilder() = default;
		BlockBuilder()          = default;

		virtual void addBlock(const BlockBuilder& block);
		void         addInstruction(const VmInstruction& instruction);

		[[nodiscard]] Block build() const;
	};

	/**
	 * @brief Creates a block of instructions.
	 */
	class VariableBlock: public BlockBuilder {
		std::deque<base::StrID> types{};

	public:
		virtual ~VariableBlock() = default;

		VariableBlock(base::StrID type_name): types({ type_name }) {}

		void addBlock(const VariableBlock& block) {
			types.insert(types.end(), block.types.begin(), block.types.end());
			BlockBuilder::addBlock(block);
		}

		void addBlock(const BlockBuilder& block) override { BlockBuilder::addBlock(block); }

		[[nodiscard]] Block build() const;
	};

	/**
	 * @brief Creates a VM function from blocks.
	 */
	class FunctionBuilder {
		std::deque<BlockBuilder> blocks;

	public:
		FunctionBuilder() = default;

		[[nodiscard]] Function build() const;
	};

	class CodeFileBuilder {
		std::deque<FunctionBuilder> functions{};
		std::deque<vm::TypeOfData>  types{};
		base::Map<base::StrID, CRef<vm::TypeOfData>> type_map{};

	public:
		CodeFileBuilder() = default;

		void addFunction(const FunctionBuilder& function);

		void addType(const vm::TypeOfData& type);

		[[nodiscard]] CodeFile build() const;
	};


}
