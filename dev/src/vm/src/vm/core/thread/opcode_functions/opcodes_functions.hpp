#pragma once

#include "../config.hpp"

#include <vm/core/process/exceptions.hpp>
#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/core/thread/low_program/utils.hpp>

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
#define HANDLE_MICRO_INSTR(opcode) static OpFun op_##opcode;
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>


#undef HANDLE_MICRO_INSTR

#define HANDLE_MICRO_INSTR(opcode) static DebugOpFun op_debug_##opcode;
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>


#undef HANDLE_MICRO_INSTR

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
#define HANDLE_MICRO_INSTR(opcode) op_##opcode,
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>


#undef HANDLE_MICRO_INSTR
		};

		/**
		 * @brief A mapping between opcode ids and debug function pointers.
		 */
		static constexpr std::array<DebugOpFun*, OP_CASES_COUNT> DEBUG_OPFUNS{
#define HANDLE_MICRO_INSTR(opcode) op_debug_##opcode,
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>


#undef HANDLE_MICRO_INSTR
		};

		/**
		 * @brief Get the Opcode from the OpFun pointer.
		 */
		static low::MicroOpcode getOpcodeFromOpFun(OpFun* fun) {
            static std::unordered_map<OpFun*, low::MicroOpcode> map {
#define HANDLE_MICRO_INSTR(instr) {op_##instr, low::instruction_tags::Op_##instr::OPCODE},
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR
            };
            return map.at(fun);
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
		static
#ifndef BUILD_TYPE_DEV_DEBUG
			__attribute__((always_inline))
#endif
			void
			performFunctionCall(
				const MicroInstruction*& instr,
				std::byte*&              local_stack,
				Frame*&                  frame,
				VMThread&                thread,
				usize                    function_id
			) {
			auto&      runtime_data     = thread.runtime_data;
			auto&      called_func      = thread.executing_program->getFunctions()[function_id];
			const bool called_rets_void = called_func.result_type->getName() == "void";

			// Size of the shared stack space between called functions.
			auto shared_stack_space_size
				= called_func.arg_size + !called_rets_void * called_func.ret_size;

			// Save current registers and flow.
			frame->instr       = instr + 1;
			frame->local_stack = local_stack;

			// Save the last frame
			auto* prev_frame = frame;

			frame++;
			frame->current_function = &called_func;

			if (frame + 1 >= runtime_data.frame_stack_end)
				throw exceptions::VMStackOverflowException();

			// Update values passed as arguments.
			instr = called_func.bc.data();
			// New local_stack address is the local_stack_head (all typed initialized by the caller
			// up to this point) - the size of ret_val and arguments passed to callee.
			local_stack += prev_frame->local_stack_head - shared_stack_space_size;

			// Assumes that local_stack_size = ret_val + passed_args + new_local_args.
			if (local_stack + called_func.local_stack_size > runtime_data.local_stack_end)
				throw exceptions::VMStackOverflowException();

			// Move shared blocks into callee's block stack and block_local_offset map.
			// This is the id of the first shared block in the caller's block_stack. If the called
			// function is non-void we also count the ret_val block.
			u64 arg_count              = called_func.parameters.size();
			u64 shared_block_count     = !called_rets_void ? arg_count + 1 : arg_count;
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

		static
#ifndef BUILD_TYPE_DEV_DEBUG
			__attribute__((always_inline))
#endif
			void
			performInit(
				[[maybe_unused]] const MicroInstruction*& instr,
				std::byte*&                               local_stack,
				Frame*&                                   frame,
				VMThread&                                 thread,
				TypeID                                    type_id
			) {
			auto type     = thread.executing_program->getTypes().at(type_id);
			auto data_ptr = local_stack + frame->local_stack_head;
			auto block    = thread.process_memory.allocateDummy(type, data_ptr);

			thread.process_memory.increaseBlockRefcount(block
			);  // so that nobody can delete our block

			// @note: We're using insert_or_assign so we don't have to remove the blocks_id to
			// local_offset mappings from the frame when we call a function. In the call, we just
			// move the local_stack_head and new inits (which will happen after we return from a
			// called function) will overwrite the old mappings.
			frame->local_offset_to_block_idx.insert_or_assign(
				frame->local_stack_head, frame->block_stack.size()
			);
			frame->block_idx_to_local_offset.insert_or_assign(
				frame->block_stack.size(), frame->local_stack_head
			);
			frame->block_stack.push_back(block);
			frame->local_stack_head += type->getSize();
		}

		static
#ifndef BUILD_TYPE_DEV_DEBUG
			__attribute__((always_inline))
#endif
			void
			performDeinit(
				[[maybe_unused]] const MicroInstruction*& instr,
				[[maybe_unused]] std::byte*&              local_stack,
				Frame*&                                   frame,
				VMThread&                                 thread
			) {
			auto block = frame->block_stack.back();
			auto type  = thread.process_memory.getBlockType(block);
			frame->block_stack.pop_back();

			// @note: Removing block_id fo local_offset mappings is not needed here, since new inits
			// will overwrite the old mappings

			thread.process_memory.freeBlockData(block);
			thread.process_memory.decreaseBlockRefcount(block);
			frame->local_stack_head -= type->getSize();
		}
	};
}  // namespace vm
