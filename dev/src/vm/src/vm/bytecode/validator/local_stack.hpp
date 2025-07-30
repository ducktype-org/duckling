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

namespace vm::code::func_validator_helpers {
	/**
	 * @brief Represents a local stack variable.
	 */
	struct LocalStackEntry {
		base::StrID      local_name;
		CRef<TypeOfData> type;

		constexpr bool operator==(const LocalStackEntry& other) const {
			return local_name == other.local_name && *type == *other.type;
		}
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
		const std::vector<LocalStackEntry>& getStackState() const {
			return stack_state;
		}

		void push(const opargs::StackLocalAny& local, const opargs::Type& type) {
			auto tod = tod_map->at(type.type_name);

			if (local_name_to_type.contains(local.var_name)) throw DuplicatedLocalNameError(local);

			stack_state.emplace_back(local.var_name, tod);
			local_name_to_type.put(local.var_name, tod);
		}

		/**
		 * @brief Pops the top element from the stack state and updates local variable mappings.
		 * Can be only used with instructions which effectively deinitialize the local stack
		 * (deinit, call_func, virtual_call and call_builtin_func)
		 */
		template<DeinitializingInstruction InstructionType>
		void pop(const InstructionType& cause) {
			if (stack_state.size() == 1) throw RetValDeinitError(cause);
			const auto& top = stack_state.back();
			local_name_to_type.erase(top.local_name);
			stack_state.pop_back();
		}

		[[nodiscard]]
		usize size() const {
			return stack_state.size();
		}

		[[nodiscard]]
		const LocalStackEntry& back() const {
			return stack_state.back();
		}

		[[nodiscard]]
		const LocalStackEntry& front() const {
			return stack_state.front();
		}

		void castPrimitive(const opargs::OpCodePrimitiveArg& local, const opargs::Type& type) {
			auto  local_name = VISIT(local, l, return l.var_name);
			auto& curr_type  = local_name_to_type.at(local_name);
			auto  new_type   = tod_map->at(type.type_name);
			curr_type        = new_type;
			for (auto& entry: stack_state)
				if (entry.local_name == local_name) entry.type = new_type;
		}

		[[nodiscard]]
		bool contains(base::StrID local_name) const {
			return local_name_to_type.contains(local_name);
		}

		[[nodiscard]]
		CRef<TypeOfData> at(base::StrID local_name) const {
			return local_name_to_type.at(local_name);
		}
	};
}
