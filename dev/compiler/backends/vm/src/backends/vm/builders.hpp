#pragma once

#include <base/ref.hpp>
#include "../../../../../../VM/src/preprocessor/parser/types_of_data.hpp"
#include <base/maps.hpp>
#include <base/string_id.hpp>
#include "code_data/opcode_args.hpp"
#include "instructions.hpp"
#include <cstdint>
#include <deque>
#include "elements.hpp"
#include <lir/lir_structure/lir_structure.hpp>

#define NOIMPL_CASE(tp, reason)                                                          \
	variant_case(tp, _) {                                                                \
		throw base::NotYetImplemented(                                                   \
			base::strConcat("Unsupported type ", base::typeName<tp>(), " for: ", reason) \
		);                                                                               \
	}

namespace compiler::backend_vm {
	class InstructionBuilder {
	public:
		[[nodiscard]] VmInstruction build() const;
	};

	/**
	 * @brief Creates a VM function from blocks.
	 */
	class FunctionBuilder {
		std::deque<VmInstruction> instructions{};
		base::StrID               name;

		struct LocalStackEntry {
			base::StrID          tp;
			usize                local_stack_position;
			usize                type_size;
			CRef<vm::TypeOfData> data_type;
		};

		std::deque<LocalStackEntry> local_stack;

		i64 max_stack_size = -1;
		i64 ret_size       = -1;

		const base::HashMap<base::StrID, vm::TypeOfData>& available_types;

	public:
		FunctionBuilder(
			base::StrID name, const base::HashMap<base::StrID, vm::TypeOfData>& available_types
		);

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
