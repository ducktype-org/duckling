#pragma once

#include <base/ref.hpp>
#include "../../../../../../VM/src/preprocessor/parser/types_of_data.hpp"
#include <base/maps.hpp>
#include <base/string_id.hpp>
#include "instructions.hpp"
#include <deque>
#include "elements.hpp"
#include <lir/lir_structure/lir_structure.hpp>

namespace compiler::backend_vm {

	// /**
	//  * @brief Creates a block of instructions.
	//  */
	// class BlockBuilder {
	// 	// @TODO: Decide if we need it.

	// 	// Jump instructions are only allowed in context
	// 	// of a single block.

	// 	std::deque<base::StrID>   types{};
	// 	std::deque<VmInstruction> instructions{};

	// public:
	// 	virtual ~BlockBuilder() = default;
	// 	BlockBuilder()          = default;

	// 	void initType(base::StrID name);
	// 	void addBlock(const BlockBuilder& block);

	// 	void addInstruction(const VmInstruction& instruction);

	// 	[[nodiscard]] virtual Block build() const;
	// };

	/**
	 * @brief Creates a VM function from blocks.
	 */
	class FunctionBuilder {
		std::deque<VmInstruction> instructions{};
		base::StrID               name;

		struct LocalStackEntry {
			base::StrID          tp;
			usize                local_stack_position;
			usize type_size;
			CRef<vm::TypeOfData> data_type;
		};

		std::deque<LocalStackEntry> local_stack;

		i64 max_stack_size = -1;
		i64 ret_size       = -1;

		const base::HashMap<base::StrID, vm::TypeOfData>& available_types;

	public:
		FunctionBuilder(base::StrID name, const base::HashMap<base::StrID, vm::TypeOfData>&);

		usize               initType(base::StrID tp);
		void                deinitType();
		[[nodiscard]] usize getLocalSize() const;

		void addInstruction(const VmInstruction& instruction);

		[[nodiscard]] Function build() const;
	};

	class CodeFileBuilder {
		std::deque<FunctionBuilder> functions{};

		std::deque<CRef<vm::TypeOfData>>           types{};
		base::HashMap<base::StrID, vm::TypeOfData> type_map{};

	public:
		CodeFileBuilder() = default;

		void addFunction(const FunctionBuilder& function);

		void addType(const vm::TypeOfData& type);

		const base::HashMap<base::StrID, vm::TypeOfData>& getAvailableTypes() const;

		[[nodiscard]] CodeFile build() const;
	};


}
