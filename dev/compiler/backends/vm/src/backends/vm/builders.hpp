#pragma once

#include <base/ref.hpp>
#include "../../../../../../VM/src/preprocessor/parser/types_of_data.hpp"
#include "base/maps.hpp"
#include "instructions.hpp"
#include <deque>
#include <vector>
#include "elements.hpp"

namespace compiler::backend_vm {

	/**
	 * @brief Creates a block of instructions.
	 */
	class BlockBuilder {
		// @TODO: Decide if we need it.

		// Jump instructions are only allowed in context
		// of a single block.

		std::deque<base::StrID>   types{};
		std::deque<VmInstruction> instructions{};

	public:
		virtual ~BlockBuilder() = default;
		BlockBuilder()          = default;

		void initType(base::StrID name);
		void addBlock(const BlockBuilder& block);

		void addInstruction(const VmInstruction& instruction);

		[[nodiscard]] virtual Block build() const;
	};

	/**
	 * @brief Creates a VM function from blocks.
	 */
	class FunctionBuilder {
		BlockBuilder body;
		base::StrID  name;

	public:
		FunctionBuilder(base::StrID name);
		void addBlock(const BlockBuilder& block);

		[[nodiscard]] Function build(const base::HashMap<base::StrID, CRef<vm::TypeOfData>>& available_types) const;
	};

	class CodeFileBuilder {
		std::deque<FunctionBuilder>                      functions{};
		std::deque<vm::TypeOfData>                       types{};
		base::HashMap<base::StrID, CRef<vm::TypeOfData>> type_map{};

	public:
		CodeFileBuilder() = default;

		void addFunction(const FunctionBuilder& function);

		void addType(const vm::TypeOfData& type);

		[[nodiscard]] CodeFile build() const;
	};


}
