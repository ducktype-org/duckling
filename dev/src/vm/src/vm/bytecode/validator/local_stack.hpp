#pragma once

#include "function_validator_helpers.hpp"

#include <base/exceptions.hpp>
#include <base/ref.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <variant>

namespace vm::code::func_validator_helpers {
	struct LocalStackEntry {
		base::StrID      local_name;
		CRef<TypeOfData> type;
	
		constexpr bool operator==(const LocalStackEntry& other) const;
	};
	
	class LocalStack {
		std::vector<LocalStackEntry> stack_state;
	
		// the following are CRefs instead of const& to allow copy/move.
	
		CRef<StableObjIdNameMap<TypeOfData>>         tod_map;
		[[maybe_unused]] CRef<TypeMetadata>          type_metadata;
		base::HashMap<base::StrID, CRef<TypeOfData>> local_name_to_type;
	
	public:
		LocalStack(const LocalStack&)            = default;
		LocalStack(LocalStack&&)                 = default;
		LocalStack& operator=(const LocalStack&) = default;
		LocalStack& operator=(LocalStack&&)      = default;
	
		LocalStack(
			const FunctionType&                   function_type,
			const StableObjIdNameMap<TypeOfData>& tod_map,
			const TypeMetadata&                   type_metadata
		):
			  tod_map(&tod_map),
			  type_metadata(&type_metadata) {
			push(base::StrID("ret_val"), function_type.result);
			for (auto [idx, param]: std::views::enumerate(function_type.parameters))
				push(base::StrID(base::strConcat("arg", idx).c_str()), param);
		}
	
		[[nodiscard]]
		const std::vector<LocalStackEntry>& getStackState() const;
	
		void push(const opargs::StackLocalAny& local, const opargs::Type& type);
	
		/**
		 * @brief Pops the top element from the stack state and updates local variable mappings.
		 * Can be only used with instructions which effectively deinitialize the local stack
		 * (deinit, call_func, virtual_call and call_builtin_func)
		 */
		template<DeinitializingInstruction InstructionType>
		void pop(const InstructionType& cause);
	
		[[nodiscard]]
		usize size() const;
	
		[[nodiscard]]
		const LocalStackEntry& back() const;
	
		[[nodiscard]]
		const LocalStackEntry& front() const;
		
		void castPrimitive(const opargs::OpCodePrimitiveArg& local, const opargs::Type& type);
	
		[[nodiscard]]
		bool contains(base::StrID local_name) const;
	
		[[nodiscard]]
		CRef<TypeOfData> at(base::StrID local_name) const;
	};
}