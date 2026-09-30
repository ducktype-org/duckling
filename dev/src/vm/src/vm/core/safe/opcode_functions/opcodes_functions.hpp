#pragma once

#include "../../config.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/raw_view.hpp>

#include <logger/logger.hpp>  // IWYU pragma: export

#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/utils.hpp>
#include <vm/core/safe/memory/local_slot_block.hpp>
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

// A dev-debug build leaves the opfun helpers out of line, so that they can be stepped into.
#ifdef BUILD_TYPE_DEV_DEBUG
	#define VM_OPFUN_INLINE
#else
	#define VM_OPFUN_INLINE __attribute__((always_inline))
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
#include <vm/core/safe/low_program/micro_instruction_definitions.def.hpp>


#undef HANDLE_MICRO_INSTR

#define HANDLE_MICRO_INSTR(opcode) static DebugOpFun op_debug_##opcode;
#include <vm/core/safe/low_program/micro_instruction_definitions.def.hpp>


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
#include <vm/core/safe/low_program/micro_instruction_definitions.def.hpp>


#undef HANDLE_MICRO_INSTR
		};

		/**
		 * @brief A mapping between opcode ids and debug function pointers.
		 */
		static constexpr std::array<DebugOpFun*, OP_CASES_COUNT> DEBUG_OPFUNS{
#define HANDLE_MICRO_INSTR(opcode) \
	low::MicroOpcode::opcode == low::MicroOpcode::check_strategy ? op_debug_nop : op_debug_##opcode,
#include <vm/core/safe/low_program/micro_instruction_definitions.def.hpp>


#undef HANDLE_MICRO_INSTR
		};

		/**
		 * @brief Get the Opcode from the OpFun pointer.
		 */
		static low::MicroOpcode getOpcodeFromOpFun(OpFun* fun) {
			static std::unordered_map<OpFun*, low::MicroOpcode> map{
#define HANDLE_MICRO_INSTR(instr) { op_##instr, low::instruction_tags::Op_##instr::OPCODE },
#include <vm/core/safe/low_program/micro_instruction_definitions.def.hpp>
#undef HANDLE_MICRO_INSTR
			};
			return map.at(fun);
		}

		/**
		 * @brief Zeroes a freshly initialized local variable and reserves its slot.
		 *
		 * @param zeroed_size Purely an optimization: always the size of `type`, which the
		 * assertion below pins. It is taken separately so that a caller knowing it at compile time
		 * zeroes with a plain store instead of reading the size back out of the type.
		 */
		static void pushLocalSlot(
			Frame* frame, TypeCRef type, byte* data, u64 zeroed_size, Block* block
		) {
			CORE_ASSERT(
				zeroed_size == type->getSize().asInt(),
				"A local variable has to be zeroed over its whole size"
			);
			std::memset(data, 0, zeroed_size);

			*frame->local_slot_stack_end = { .type = type.get(), .data = data, .block = block };
			frame->local_slot_stack_end += 1;
		}

		/**
		 * @brief Drops the topmost local variable slot without touching its block.
		 */
		static void popLocalSlot(Frame* frame) { frame->local_slot_stack_end -= 1; }

		/**
		 * @brief The topmost local variable slot.
		 */
		static const LocalSlot& topLocalSlot(Frame* frame) {
			return frame->local_slot_stack_end[-1];
		}

		/// `createLocalSlotBlock`, kept out of line: it runs at most once per variable,
		/// so it must not bloat the opfuns.
		[[gnu::noinline]]
		static Ref<Block> createLocalBlock(Frame* frame, SafeVMThread& thread, u64 slot_index) {
			return createLocalSlotBlock(*frame, thread.process_memory, slot_index);
		}

		/**
		 * @brief Resolves a block place argument - an index into the frame's local slot stack,
		 * or into the global block buffer when the highest bit is set - to a block.
		 *
		 * Globals always have their blocks; a local gets one created on the spot the first time
		 * one is needed.
		 */
		[[nodiscard]]
		VM_OPFUN_INLINE static Ref<Block> readBlockRefFromArg(
			Frame* frame, SafeVMThread& thread, u64 arg
		) {
			// The highest bit tells globals apart from locals, the rest is the index.
			const bool is_global = (arg >> 63) != 0;
			const u64  index     = arg & ~(1ULL << 63);

			if (is_global) return { thread.runtime_data.global_block_ref_buffer_base[index] };

			if (frame->local_slot_stack_base[index].block == nullptr) [[unlikely]]
				return createLocalBlock(frame, thread, index);

			return { frame->local_slot_stack_base[index].block };
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
		static VM_OPFUN_INLINE void performFunctionCall(
			const MicroInstruction*& instr,
			byte*&                   local_stack,
			Frame*&                  frame,
			SafeVMThread&            thread,
			usize                    function_id,
			u64                      callee_stack_distance,
			u64                      instruction_size
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

			auto arg_count           = called_func.parameters.size();
			auto ret_count           = called_func.result_types.size();
			auto shared_blocks_count = arg_count + ret_count;
			u64  prev_frame_slot_count
				= u64(frame->local_slot_stack_end - frame->local_slot_stack_base);

			// Save current registers and flow.
			frame->instr       = instr + instruction_size;
			frame->local_stack = local_stack;

			// Save the last frame
			auto* prev_frame = frame;

			frame++;

			if (frame + 1 >= runtime_data.frame_stack_end)
				throw exceptions::VMStackOverflowException();

			frame->current_function = &called_func;

			// Update values passed as arguments.
			instr = called_func.bc.data();
			// The callee's local stack starts where the space shared with the caller (its return
			// values followed by its arguments) begins.
			local_stack += callee_stack_distance;
			frame->local_slot_stack_base
				= prev_frame->local_slot_stack_base + (prev_frame_slot_count - shared_blocks_count);

			// Cross-checks the distance, computed at lowering time, against the address the
			// caller's own `init` recorded for the first slot they share.
			CORE_ASSERT(
				shared_blocks_count == 0 || frame->local_slot_stack_base[0].data == local_stack,
				"The callee's shared slots have to start where its local stack does"
			);

			// Assumes that local_stack_size = ret_val + passed_args + new_local_args.
			if (local_stack + called_func.local_stack_size >= runtime_data.local_stack_end)
				throw exceptions::VMStackOverflowException();
			if (frame->local_slot_stack_base + called_func.local_slot_count
			    >= runtime_data.slot_stack_end)
				throw exceptions::VMStackOverflowException();

			frame->local_slot_stack_end = prev_frame->local_slot_stack_end;

			// Remove the argument slots from the caller's slot stack. Only the return value stays
			// there.
			// @note: We require that the callee can't deinitialize the return value passed by the
			// caller.
			prev_frame->local_slot_stack_end -= arg_count;
		}

		/**
		 * @brief Initializes a local variable together with its block.
		 *
		 * Unlike `init_off_type`, which leaves the slot blockless until something asks for a
		 * block, this creates one up front. Nothing the lowering emits for ordinary programs
		 * takes this path any more; the only caller left is `init_vmvalptr_off`, emitted for the
		 * synthetic start function.
		 */
		static VM_OPFUN_INLINE void performInit(
			byte*& local_stack, Frame*& frame, SafeVMThread& thread, u64 byte_offset, TypeCRef type
		) {
			auto data_ptr = local_stack + byte_offset;
			// The variable is zeroed by `pushLocalSlot`, so the block only adopts its bytes.
			auto block = thread.process_memory.adoptDummy(type, data_ptr);

			thread.process_memory.increaseBlockRefcount(block
			);  // so that nobody can delete our block

			pushLocalSlot(frame, type, data_ptr, type->getSize().asInt(), block.get());
		}

		static VM_OPFUN_INLINE void performDeinit(Frame*& frame, SafeVMThread& thread) {
			// A variable that never needed a block has none to free.
			if (Block* block = frame->local_slot_stack_end[-1].block) {
				thread.process_memory.freeBlockData(block);
				thread.process_memory.decreaseBlockRefcount(block);
			}

			popLocalSlot(frame);
		}

		static VM_OPFUN_INLINE void setVariantType(
			SafeVMThread& thread, Pointer variant_pointer, TypeCRef wanted_type, TypeCRef variant_type
		) {
			auto variant_type_tag_size = variant_type->getTypeTagSizeBytes().value();

			// Set the view block
			auto nested_data_ptr = variant_pointer;
			nested_data_ptr.movePointer(variant_type_tag_size.asInt());
			thread.process_memory.setNestedViewBlock(
				nested_data_ptr.getBlock(), nested_data_ptr.getOffset(), wanted_type
			);

			// Find type index
			auto  alternatives      = variant_type->getVariantAlternatives().value();
			usize alternative_index = 0;

			// @TODO: #3374 - Make usage of type 0 be accounted here as well
			// Also, optimize this...
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

		static VM_OPFUN_INLINE Pointer getVariantPtr(
			SafeVMThread& thread, Pointer variant_pointer, TypeCRef wanted_type, TypeCRef variant_type
		) {
			auto view_block_ref = thread.process_memory.getNestedViewBlock(
				variant_pointer.getBlock(),
				variant_pointer.getOffset() + variant_type->getTypeTagSizeBytes()->asInt(),
				wanted_type
			);

			match_optional(view_block_ref.toOpt()) {
				opt_some(view_block) { return Pointer{ view_block, 0 }; }
				opt_none { return Pointer::null(); }
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief A null cpointer (e.g. a default-initialized local) must not be dereferenced.
		 */
		static VM_OPFUN_INLINE void assertCPtrNotNull(void* cptr) {
			if (cptr == nullptr) throw vm::exceptions::VMFFIError("Accessed null CPointer");
		}
	};

}  // namespace vm
