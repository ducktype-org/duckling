#pragma once

#include "../config.hpp"

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/vmthread.hpp>

#ifdef USE_TAIL_CALLS
	#define OPFUN_ARGS OPFUN_TC_ARGS
#else
	#define OPFUN_ARGS OPFUN_REF_ARGS
#endif

#ifdef USE_TAIL_CALLS
	#define RETURN_TYPE RETURN_TYPE_OPFUN_TC
#else
	#define RETURN_TYPE RETURN_TYPE_OPFUN_REF
#endif

namespace vm {
	using DebugOpFun = void(OPFUN_REF_ARGS);
	using OpFun      = void(OPFUN_ARGS);

	/**
	 * @brief A class that contains all opcode functions implementations.
	 * It is created to be a friend of the Thread and Process classes,
	 * so the instructions have access to the private members of these classes.
	 */
	class OpFuns final {
	public:
#define HANDLE_OPCODE(opcode) static OpFun op_##opcode;
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE

#define HANDLE_OPCODE(opcode) static DebugOpFun op_debug_##opcode;
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE

		// NOLINTBEGIN(readability-identifier-naming)
		// Opcodes utilities functions (named the similar way as all OpFuns)
		static OpFun handle_execution_break;
		static OpFun save_execution_state;
		// NOLINTEND(readability-identifier-naming)

		/**
		 * @brief A mapping between opcode ids and function pointers.
		 *
		 * @warning Ordering of elements must stay the same as in vm::OpcodeFix8
		 */
		static constexpr std::array<OpFun*, OP_CASES_COUNT> OPFUNS{
#define HANDLE_OPCODE(opcode) op_##opcode,
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE
		};

		/**
		 * @brief A mapping between opcode ids and debug function pointers.
		 */
		static constexpr std::array<DebugOpFun*, OP_CASES_COUNT> DEBUG_OPFUNS{
#define HANDLE_OPCODE(opcode) op_debug_##opcode,
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE
		};

		/**
		 * @brief Get the Opcode from the OpFun pointer.
		 */
		static u16 getOpcodeFromOpFun(OpFun* fun) {
			for (u16 i = 0; i < OP_CASES_COUNT; i++)
				if (OPFUNS.at(i) == fun) return i;
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Prepares the execution variables and frames for a call to a function with a
		 * specified id.
		 *
		 * After this function:
		 * `instr` should be pointer to the instruction in the new function,
		 * `frame` should be pointer to the next frame,
		 * `local_stack` should be pointer to the local stack of the new function.
		 * Old values of `instr` nad `local_stack` should be saved on the frame of the caller.
		 *
		 * @note The function has to be inlined since it's used by the `call_func` and
		 * `virtual_call` opcodes and breaks tailcalling of opcode function if not inlined.
		 */
		static __attribute__((always_inline)) void performFunctionCall(
			const Fix8Instruction*& instr,
			std::byte*&             local_stack,
			Frame*&                 frame,
			VMThread&               thread,
			usize                   function_id
		) {
			auto& runtime_data = thread.runtime_data;
			auto& called_func  = thread.executing_program->functions[function_id];

			// Size of the shared stack space between called functions.
			auto shared_stack_space_size = called_func.arg_size + called_func.ret_size;

			// Save current registers and flow.
			frame->instr                = instr + 1;
			frame->local_stack          = local_stack;
			frame->called_func_arg_size = called_func.arg_size;
			frame->called_func_ret_size = called_func.ret_size;

			// Save the last frame
			auto* prev_frame = frame;

			frame++;

			if (frame + 1 >= runtime_data.frame_stack_end) CORE_PANIC("VM stack overflow.");

			// Update values passed as arguments.
			instr = called_func.bc.data();
			// New local_stack address is the local_stack_head (all typed initialized by the caller
			// up to this point) - the size of ret_val and arguments passed to callee.
			local_stack += prev_frame->local_stack_head - shared_stack_space_size;

			// Assumes that local_stack_size = ret_val + passed_args + new_local_args.
			if (local_stack + called_func.local_stack_size > runtime_data.local_stack_end)
				CORE_PANIC("VM stack overflow.");

			// Move shared blocks into callee's block stack and block_local_offset map.
			// This is the id of the first shared block in the caller's block_stack. If the called
			// function is non-void we also count the ret_val block.
			auto called_func_type = thread.executing_program->types->at(called_func.name);
			u64  arg_count
				= called_func_type->getParameterCount().expect("Parameter count not set!");
			u64 shared_block_count     = called_func.ret_size != 0 ? arg_count + 1 : arg_count;
			u64 shared_blocks_start_ix = prev_frame->block_stack.size() - shared_block_count;

			frame->local_stack_head = shared_stack_space_size;
			for (u64 i = shared_blocks_start_ix; i < prev_frame->block_stack.size(); i++) {
				frame->block_stack.push_back(prev_frame->block_stack[i]);
				auto callers_local_offset = prev_frame->block_idx_to_local_offset[i];
				// This points to the ret_val offset.
				auto offset_before_ret_val = prev_frame->local_stack_head - shared_stack_space_size;
				auto new_offset            = callers_local_offset - offset_before_ret_val;

				frame->local_offset_to_block_idx.put(new_offset, i - shared_blocks_start_ix);
				frame->block_idx_to_local_offset.put(i - shared_blocks_start_ix, new_offset);
			}

			// Remove the argument blocks from caller's block stack. Only the return value stays in
			// the block stack.
			// @note: We require that the callee can't deinitialize the return value passed by the
			// caller.
			prev_frame->local_stack_head -= called_func.arg_size;
			for (u64 i = 0; i < arg_count; i++) {
				prev_frame->block_stack.pop_back();
				// @note: Removing block_id to local_offset mappings from the frame is not needed,
				// since a new init (after returning from a called function) to the same
				// offset/block_idx will overwrite the old values.
			}
		}
	};
}  // namespace vm
