#pragma once

#include "../config.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/raw_view.hpp>

#include <logger/logger.hpp>  // IWYU pragma: export

#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/utils.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions_utils.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/module_flags/module_flags.hpp>

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
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>


#undef HANDLE_MICRO_INSTR

#define HANDLE_MICRO_INSTR(opcode) static DebugOpFun op_debug_##opcode;
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>


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
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>


#undef HANDLE_MICRO_INSTR
		};

		/**
		 * @brief A mapping between opcode ids and debug function pointers.
		 */
		static constexpr std::array<DebugOpFun*, OP_CASES_COUNT> DEBUG_OPFUNS{
#define HANDLE_MICRO_INSTR(opcode) op_debug_##opcode,
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>


#undef HANDLE_MICRO_INSTR
		};

		/**
		 * @brief Get the Opcode from the OpFun pointer.
		 */
		static low::MicroOpcode getOpcodeFromOpFun(OpFun* fun) {
			static std::unordered_map<OpFun*, low::MicroOpcode> map{
#define HANDLE_MICRO_INSTR(instr) { op_##instr, low::instruction_tags::Op_##instr::OPCODE },
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
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
				SafeVMThread&            thread,
				usize                    function_id
			) {
			auto& runtime_data = thread.runtime_data;
			auto& called_func  = thread.process_program->getFunctions()[function_id];

			if constexpr (ENABLE_VM_DETAIL_LOGGING)
				CORE_DEV_LOG(
					DVMDetails,
					"function, ",
					called_func.name.str(),
					", ",
					thread.getThreadID().asInt(),
					";\n"
				);

			// Size of the shared stack space between called functions.
			auto shared_stack_space_size = called_func.arg_size + called_func.ret_size;

			auto arg_count           = called_func.parameters.size();
			auto ret_count           = called_func.result_types.size();
			auto shared_blocks_count = arg_count + ret_count;
			u64  prev_frame_block_ref_count
				= u64(frame->local_block_ref_stack_end - frame->local_block_ref_stack_base);

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
			// up to this point) - the size of ret_vals and arguments passed to callee.
			local_stack += prev_frame->local_stack_head - shared_stack_space_size;
			frame->local_block_ref_stack_base = prev_frame->local_block_ref_stack_base
			                                  + (prev_frame_block_ref_count - shared_blocks_count);

			// Assumes that local_stack_size = ret_val + passed_args + new_local_args.
			if (local_stack + called_func.local_stack_size >= runtime_data.local_stack_end)
				throw exceptions::VMStackOverflowException();
			if (frame->local_block_ref_stack_base + called_func.local_block_count
			    >= runtime_data.block_ref_stack_end)
				throw exceptions::VMStackOverflowException();

			frame->local_stack_head          = shared_stack_space_size;
			frame->local_block_ref_stack_end = prev_frame->local_block_ref_stack_end;

			// Remove the argument blocks from caller's block stack. Only the return value stays in
			// the block stack.
			// @note: We require that the callee can't deinitialize the return value passed by the
			// caller.
			prev_frame->local_block_ref_stack_end -= arg_count;
			prev_frame->local_stack_head -= called_func.arg_size;
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
				SafeVMThread&                             thread,
				TypeCRef                                  type
			) {
			auto data_ptr = local_stack + frame->local_stack_head;
			auto block    = thread.process_memory.allocateDummy(type, data_ptr);

			auto shadow_data_ptr
				= thread.runtime_data.shadow_local_stack_base + frame->local_stack_head;
			auto shadow_block = thread.safe_process.shadow_memory.allocateDummy(type, shadow_data_ptr);

			thread.process_memory.increaseBlockRefcount(block);
			thread.safe_process.shadow_memory.increaseBlockRefcount(shadow_block);

			*frame->local_block_ref_stack_end = block.get();
			frame->local_block_ref_stack_end += 1;
			frame->local_stack_head += type->getSize().asInt();
		}

		static
#ifndef BUILD_TYPE_DEV_DEBUG
			__attribute__((always_inline))
#endif
			void
			performDeinit(Frame*& frame, SafeVMThread& thread) {
			auto block = frame->local_block_ref_stack_end[-1];
			auto type  = thread.process_memory.getBlockType(block);

			auto block_id     = thread.process_memory.requestBlockID(block);
			auto shadow_block = thread.safe_process.shadow_memory.getBlock(block_id);

			thread.process_memory.freeBlockData(block);
			thread.process_memory.decreaseBlockRefcount(block);

			thread.safe_process.shadow_memory.freeBlockData(shadow_block);
			thread.safe_process.shadow_memory.decreaseBlockRefcount(shadow_block);

			frame->local_stack_head -= type->getSize().asInt();
			frame->local_block_ref_stack_end -= 1;
		}

		static
#ifndef BUILD_TYPE_DEV_DEBUG
			__attribute__((always_inline))
#endif
			void
			setVariantType(
				SafeVMThread& thread,
				Pointer       variant_pointer,
				TypeCRef      wanted_type,
				TypeCRef      variant_type
			) {
			auto variant_type_tag_size = variant_type->getTypeTagSizeBytes().value();

			// Set the view block
			auto nested_data_ptr = variant_pointer;
			nested_data_ptr.movePointer(static_cast<i64>(variant_type_tag_size));
			thread.process_memory.setNestedViewBlock(nested_data_ptr, wanted_type);

			// Find type index
			auto  alternatives      = variant_type->getVariantAlternatives().value();
			usize alternative_index = 0;
			for (const auto& [idx, alt]: std::views::enumerate(alternatives))
				if (alt == wanted_type) alternative_index = static_cast<usize>(idx);

			// Write the type tag
			auto variant_block_data_view
				= thread.process_memory.getBlockViewUnsafe(variant_pointer.getBlock());
			auto variant_data_view = base::ModRawView(
				variant_block_data_view.getBegin() + variant_pointer.getOffset(),
				variant_type->getSize().asInt()
			);

			switch (variant_type_tag_size.asInt()) {
			case 1:
				// byte, using uint8_t below since byte is not std::integral
				writeToView(variant_data_view, base::safeIntConv<uint8_t>(alternative_index));
				break;
			case 2:
				writeToView(variant_data_view, base::safeIntConv<u16>(alternative_index));
				break;
			case 4:
				writeToView(variant_data_view, base::safeIntConv<u32>(alternative_index));
				break;
			case 8:
				writeToView(variant_data_view, base::safeIntConv<u64>(alternative_index));
				break;
			default:
				CORE_PANIC("Invalid variant size: ", variant_type_tag_size.asInt());
			}
		}

		static
#ifndef BUILD_TYPE_DEV_DEBUG
			__attribute__((always_inline))
#endif
			Pointer
			getVariantPtr(
				SafeVMThread& thread,
				Pointer       variant_pointer,
				TypeCRef      wanted_type,
				TypeCRef      variant_type
			) {

			auto view_block_ref = thread.process_memory.getNestedViewBlock(
				variant_pointer.movedPointer(static_cast<i64>(*variant_type->getTypeTagSizeBytes())),
				wanted_type
			);

			match_optional(view_block_ref.toOpt()) {
				opt_some(view_block) { return Pointer{ view_block, 0 }; }
				opt_none { return Pointer::null(); }
			}
			CORE_UNREACHABLE();
		}
	};

}  // namespace vm
