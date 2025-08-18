
/**
 * @file opcodes_functions_impl_base.hpp
 * @brief The opcodes functions implementations.
 *
 * @warning Do not include this file directly. Include `opcodes_functions_impl_exec.cpp` or
 * `opcodes_functions_impl_debug.cpp` instead.
 *
 * Motivation: each opcode that thread executes has its own function that is called to
 * perform the opcode operation. They are called "OpFuns". At the end of each
 * function, there is a call to `FUNCTION_CONT` macro that tail calls (in TC version)
 * to the next instruction. But in debug mode, if we want to step only one instruction,
 * we need OpFun that doesn't tail call to the next instruction, but pauses the execution instead.
 *
 * This file has **two versions**. One is for the OpFuns implementation and the other is
 * for the debug version of the OpFuns (DebugOpFun), which is a copy of the OpFuns but
 * with different `FUNCTION_CONT` and `ARGS`. This file **should not be included**.
 * If you want to include the OpFuns, include `opcodes_functions.hpp` or
 * `opcodes_functions_debug.hpp`.
 *
 * If `DEBUG_OPCODES` is defined, the debug version will be included, otherwise the regular
 * version will be included. This way we also have C++ language server support while writing
 * the code.
 *
 * @warning This file has to contain only the OpFuns. Any other functions will be declared
 * and defined twice leading to multiple definition error. Utilities functions are defined in
 * `opcodes_functions_utils.hpp`.
 */

#include "opcodes_functions_utils.hpp"

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>
#include <base/ints.hpp>
#include <base/variant.hpp>

#include <vm/core/process/builtin_functions.hpp>
#include <vm/core/process/exceptions.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/core/thread/vmvalue.hpp>
#include <vm/utils/interpret.hpp>

#ifdef DEBUG_OPCODES
	#define OPCODE_NAME(name)                  op_debug_##name
	#define FUNCTION_ARGS                      OPFUN_REF_ARGS
	#define FUNCTION_CONT(step)                instr += step;
	#define FUNCTION_CONT_CHECK_STRATEGY(step) instr += step;
#else
	#define OPCODE_NAME(name)                  op_##name
	#define FUNCTION_ARGS                      OPFUN_ARGS
	#define FUNCTION_CONT(step)                OPFUN_CONT(step)
	#define FUNCTION_CONT_CHECK_STRATEGY(step) OPFUN_CONT_CHECK_STRATEGY(step)
#endif

namespace vm {

	// NOTE: functions that implement opcodes (opfunctions) must be done this way:
	//
	// RETURN_TYPE OpFuns::op_<opcode_name>(FUNCTION_ARGS) {
	//  {
	//    <function_body>
	//  }
	//  FUNCTION_CONT(<step>);
	// }
	//
	// Function body must be separated from the scope of FUNCTION_CONT to make sure
	// that all its destructors have been called before invoking next tail call.
	// Otherwise, the compiler may get confused and may schedule destructors from
	// the body after the next tail call, which then becomes a regular function
	// call and may cause the stack to explode.

	// `op_exit` is the only opcode without the `FUNCTION_CONT` or `FUNCTION_CONT_CHECK_STRATEGY`
	// macro. This means, every other will jump to the next instruction at the end of it with
	// `FUNCTION_CONT`/`FUNCTION_CONT_CHECK_STRATEGY`, so the the only way to end execution is to
	// use this opcode. It also requires different macro surrounding the function call in the
	// computed goto's and switch case, because in those approaches we can't end execution from
	// within the function, but we have to add some instructions on the outside of it. Hence we use
	// the `OP_CASE_END` macro that adds `goto End` instruction, residing after opcode function,
	// inside interpreter loop.
	RETURN_TYPE OpFuns::OPCODE_NAME(exit)(FUNCTION_ARGS) { IF_TC(return;) }

#define DEFINE_MOVE_OPS(BITS_SIZE, TYPE)                                                          \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {                      \
		{ derefStack<TYPE>(local_stack, instr->arg0) = safeReadBytes<TYPE>(instr->arg1); }        \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_g##BITS_SIZE##_imm)(FUNCTION_ARGS) {                      \
		{ DEREF_GLOBAL_RAW_UNSAFE(TYPE, instr->arg0) = safeReadBytes<TYPE>(instr->arg1); }        \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {             \
		{                                                                                         \
			auto dst_block = frame->block_stack[frame->local_offset_to_block_idx[instr->arg0]];   \
			auto src_block = frame->block_stack[frame->local_offset_to_block_idx[instr->arg1]];   \
			thread.process_memory.copyPointedData(                                                \
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block) \
			);                                                                                    \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_g##BITS_SIZE##_g##BITS_SIZE)(FUNCTION_ARGS) {             \
		{                                                                                         \
			auto dst_block = DEREF_GLOBAL(instr->arg0);                                           \
			auto src_block = DEREF_GLOBAL(instr->arg1);                                           \
			thread.process_memory.copyPointedData(                                                \
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block) \
			);                                                                                    \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_g##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {             \
		{                                                                                         \
			auto dst_block = DEREF_GLOBAL(instr->arg0);                                           \
			auto src_block = frame->block_stack[frame->local_offset_to_block_idx[instr->arg1]];   \
			thread.process_memory.copyPointedData(                                                \
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block) \
			);                                                                                    \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l##BITS_SIZE##_g##BITS_SIZE)(FUNCTION_ARGS) {             \
		{                                                                                         \
			auto dst_block = frame->block_stack[frame->local_offset_to_block_idx[instr->arg0]];   \
			auto src_block = DEREF_GLOBAL(instr->arg1);                                           \
			thread.process_memory.copyPointedData(                                                \
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block) \
			);                                                                                    \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(cmov_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {            \
		{                                                                                         \
			if (frame->flags.flag) {                                                              \
				auto dst_block                                                                    \
					= frame->block_stack[frame->local_offset_to_block_idx[instr->arg0]];          \
				auto src_block                                                                    \
					= frame->block_stack[frame->local_offset_to_block_idx[instr->arg1]];          \
				thread.process_memory.copyPointedData(                                            \
					{ dst_block, 0 },                                                             \
					{ src_block, 0 },                                                             \
					thread.process_memory.getBlockType(dst_block)                                 \
				);                                                                                \
			}                                                                                     \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}

	DEFINE_MOVE_OPS(64, i64)
	DEFINE_MOVE_OPS(32, i32)
	DEFINE_MOVE_OPS(16, i16)
	DEFINE_MOVE_OPS(8, std::int8_t)

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lptr_gptr)(FUNCTION_ARGS) {
		{
			auto& dst_ptr = derefStack<Pointer>(local_stack, instr->arg0);
			auto  src_ptr = DEREF_GLOBAL_RAW_UNSAFE(Pointer, instr->arg1);
			thread.process_memory.setPointer(dst_ptr, src_ptr);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_gptr_lptr)(FUNCTION_ARGS) {
		{
			auto& dst_ptr = DEREF_GLOBAL_RAW_UNSAFE(Pointer, instr->arg0);
			auto  src_ptr = derefStack<Pointer>(local_stack, instr->arg1);
			thread.process_memory.setPointer(dst_ptr, src_ptr);
		}

		FUNCTION_CONT(1);
	}

#define DEFINE_ARITHMETIC_OP(NAME, BITS_SIZE, TYPE, OP)                                     \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {    \
		{                                                                                   \
			derefStack<TYPE>(local_stack, instr->arg0)                                      \
				OP derefStack<TYPE>(local_stack, instr->arg1);                              \
		}                                                                                   \
		FUNCTION_CONT(1);                                                                   \
	}                                                                                       \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {             \
		{ derefStack<TYPE>(local_stack, instr->arg0) OP safeReadBytes<TYPE>(instr->arg1); } \
		FUNCTION_CONT(1);                                                                   \
	}

#define DEFINE_DIVISION_OP(NAME, BITS_SIZE, TYPE)                                        \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) { \
		{                                                                                \
			auto& lhs = derefStack<TYPE>(local_stack, instr->arg0);                      \
			auto& rhs = derefStack<TYPE>(local_stack, instr->arg1);                      \
			if (rhs == 0) throw exceptions::VMZeroDivisionException();                   \
			lhs /= rhs;                                                                  \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}                                                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {          \
		{                                                                                \
			auto& lhs = derefStack<TYPE>(local_stack, instr->arg0);                      \
			auto  rhs = safeReadBytes<TYPE>(instr->arg1);                                \
			if (rhs == 0) throw exceptions::VMZeroDivisionException();                   \
			lhs /= rhs;                                                                  \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}


	DEFINE_ARITHMETIC_OP(add, 64, i64, +=)
	DEFINE_ARITHMETIC_OP(add, 32, i32, +=)
	DEFINE_ARITHMETIC_OP(sub, 64, i64, -=)
	DEFINE_ARITHMETIC_OP(sub, 32, i32, -=)
	DEFINE_ARITHMETIC_OP(mul, 64, i64, *=)
	DEFINE_ARITHMETIC_OP(mul, 32, i32, *=)
	DEFINE_ARITHMETIC_OP(mod, 64, i64, %=)
	DEFINE_ARITHMETIC_OP(mod, 32, i32, %=)
	DEFINE_DIVISION_OP(div, 64, i64)
	DEFINE_DIVISION_OP(div, 32, i32)

	DEFINE_ARITHMETIC_OP(fadd, 64, double, +=)
	DEFINE_ARITHMETIC_OP(fadd, 32, float, +=)
	DEFINE_ARITHMETIC_OP(fsub, 64, double, -=)
	DEFINE_ARITHMETIC_OP(fsub, 32, float, -=)
	DEFINE_ARITHMETIC_OP(fmul, 64, double, *=)
	DEFINE_ARITHMETIC_OP(fmul, 32, float, *=)
	DEFINE_ARITHMETIC_OP(fdiv, 64, double, /=)
	DEFINE_ARITHMETIC_OP(fdiv, 32, float, /=)


	DEFINE_ARITHMETIC_OP(umul, 64, u64, *=)
	DEFINE_ARITHMETIC_OP(umul, 32, u32, *=)
	DEFINE_ARITHMETIC_OP(umod, 64, u64, %=)
	DEFINE_ARITHMETIC_OP(umod, 32, u32, %=)
	DEFINE_DIVISION_OP(udiv, 64, u64)
	DEFINE_DIVISION_OP(udiv, 32, u32)

#define DEFINE_BOOLEAN_OP(NAME, OP)                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l8##_l8)(FUNCTION_ARGS) {                            \
		{                                                                                       \
			derefStack<u8>(local_stack, instr->arg0)                                            \
				= static_cast<u8>((derefStack<u8>(local_stack, instr->arg0) != u8{ 0 })         \
			                          OP(derefStack<u8>(local_stack, instr->arg1) != u8{ 0 })); \
		}                                                                                       \
		FUNCTION_CONT(1);                                                                       \
	}                                                                                           \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l8##_imm)(FUNCTION_ARGS) {                           \
		{                                                                                       \
			derefStack<u8>(local_stack, instr->arg0)                                            \
				= static_cast<u8>((derefStack<u8>(local_stack, instr->arg0) != u8{ 0 })         \
			                          OP(safeReadBytes<u8>(instr->arg1) != u8{ 0 }));           \
		}                                                                                       \
		FUNCTION_CONT(1);                                                                       \
	}

	DEFINE_BOOLEAN_OP(log_and, &&)
	DEFINE_BOOLEAN_OP(log_or, ||)
	DEFINE_BOOLEAN_OP(log_xor, !=)

	RETURN_TYPE OpFuns::OPCODE_NAME(log_not_l8)(FUNCTION_ARGS) {
		{
			bool result = (derefStack<u8>(local_stack, instr->arg0) == u8{ 0 });
			derefStack<u8>(local_stack, instr->arg0) = (result ? u8{ 1 } : u8{ 0 });
		}
		FUNCTION_CONT(1);
	}

#define DEFINE_COMPARISON_OP(NAME, BITS_SIZE, TYPE, OP)                                           \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {          \
		{                                                                                         \
			frame->flags.flag = derefStack<TYPE>(local_stack, instr->arg0)                        \
				OP derefStack<TYPE>(local_stack, instr->arg1);                                    \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {                   \
		{                                                                                         \
			frame->flags.flag                                                                     \
				= derefStack<TYPE>(local_stack, instr->arg0) OP safeReadBytes<TYPE>(instr->arg1); \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}

	DEFINE_COMPARISON_OP(cmpEq, 64, i64, ==)
	DEFINE_COMPARISON_OP(cmpG, 64, i64, >)
	DEFINE_COMPARISON_OP(ucmpG, 64, u64, >)
	DEFINE_COMPARISON_OP(cmpEq, 32, i32, ==)
	DEFINE_COMPARISON_OP(cmpG, 32, i32, >)
	DEFINE_COMPARISON_OP(ucmpG, 32, u32, >)
	DEFINE_COMPARISON_OP(cmpEq, 8, std::int8_t, ==)
	DEFINE_COMPARISON_OP(cmpG, 8, std::int8_t, >)
	DEFINE_COMPARISON_OP(ucmpG, 8, std::uint8_t, >)

	RETURN_TYPE OpFuns::OPCODE_NAME(cmpNull_lptr)(FUNCTION_ARGS) {
		{
			auto pointer      = derefStack<Pointer>(local_stack, instr->arg0);
			frame->flags.flag = pointer.isNull();
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(jmp_label)(FUNCTION_ARGS) {
		{ instr += instr->arg0; }
		FUNCTION_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(jmpIf_label)(FUNCTION_ARGS) {
		{
			if (frame->flags.flag) instr += instr->arg0;
		}
		FUNCTION_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(jmpIfNot_label)(FUNCTION_ARGS) {
		{
			if (!frame->flags.flag) instr += instr->arg0;
		}
		FUNCTION_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(neg_l64)(FUNCTION_ARGS) {
		{ derefStack<i64>(local_stack, instr->arg0) *= -1; }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(neg_l32)(FUNCTION_ARGS) {
		{ derefStack<i32>(local_stack, instr->arg0) *= -1; }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fneg_l64)(FUNCTION_ARGS) {
		{ derefStack<double>(local_stack, instr->arg0) *= -1.0; }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fneg_l32)(FUNCTION_ARGS) {
		{ derefStack<float>(local_stack, instr->arg0) *= -1.0f; }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(call_func)(FUNCTION_ARGS) {
		{ performFunctionCall(instr, local_stack, frame, thread, static_cast<usize>(instr->arg0)); }
		// After acquiring the `executing_code` of the new function we have instruction pointer
		// (`instr`) pointing at the first instruction of the new function, so moving forward by one
		// would mean that we skipped the first instruction. That's why we move forward zero
		// instructions. For future returns, the first instruction that should be executed after
		// call is saved on frame so that op_ret's have to move forward zero instructions after
		// restoring `instr` from frame.
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(call_builtin_func)(FUNCTION_ARGS) {
		{
			auto builtin_id         = static_cast<builtins::BuiltinFunctionID>(instr->arg0);
			auto function_type      = builtins::getBuiltinFunctionType(builtin_id);
			auto real_function_type = thread.executing_program->types->at(function_type->name);
			auto arg_count          = function_type->parameters.size();

			std::vector<Box<VmValue>> args;
			u64                       first_arg_idx = frame->block_stack.size() - arg_count;

			// Create VmValue objects from local arguments.
			for (u64 i = 0; i < arg_count; i++) {
				const base::StrID arg_type  = function_type->parameters[i];
				TypeCRef          real_type = thread.executing_program->types->at(arg_type);
				auto              block     = frame->block_stack[first_arg_idx + i];
				args.push_back(thread.process.createOwnedVmValue(real_type, Pointer(block, 0)));
			}

			base::Optional<Box<VmValue>> return_value = builtins::callBuiltinFunction(
				builtin_id, real_function_type, thread.process, thread, args
			);

			if (return_value.has_value()) {
				auto value = std::move(return_value.value());
				value->exportData(Pointer(frame->block_stack[first_arg_idx - 1], 0));
				value->freeData();
			}
			for (auto& vm_value: args) vm_value->freeData();

			// Similar as in call_func, but we deinit the arguments blocks as well,
			// but without the return value.
			for (u64 i = 0; i < arg_count; i++) {
				auto block = frame->block_stack.back();
				frame->block_stack.pop_back();
				thread.process_memory.freeBlock(block);
			}
			if (arg_count > 0)
				frame->local_stack_head = frame->block_idx_to_local_offset[first_arg_idx];
		}

		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(virtual_call_lptr_method)(FUNCTION_ARGS) {
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg0);
			// Objects are guaranteed to hold a inheritance metadata pointes as their first field.
			// This is verified by static verification.
			const vm::Type* inh_meta_pointer = *reinterpret_cast<const vm::Type**>(
				thread.process_memory.getPointerData(pointer, sizeof(Type*)).getBegin()
			);
			const vm::InheritanceMetadata& inh_metadata
				= *inh_meta_pointer->getInheritanceMetadata().value();

			auto  method_name         = thread.executing_program->method_name_pool[instr->arg1];
			auto  implementation_name = inh_metadata.vtable[method_name]->getName();
			usize function_id = *thread.executing_program->functions.idOf(implementation_name);

			performFunctionCall(instr, local_stack, frame, thread, function_id);
		}
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret_tailcall_func)(FUNCTION_ARGS) {
		{
			auto  function_id = static_cast<usize>(instr->arg0);
			auto& function    = thread.executing_program->functions[function_id];
			instr             = function.bc.data();

			if (local_stack + function.local_stack_size > thread.runtime_data.local_stack_end)
				throw exceptions::VMStackOverflowException();
		}
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret)(FUNCTION_ARGS) {
		{
			// Frame of the function we're returning from.
			auto* callee_frame = frame;

			// We have to update values passed in arguments.
			// Old `instr` and `local_stack` are stored on the previous frame.
			// Previous frame is just before current frame in the array, so that
			// substracting one from the pointer will give us the previous frame.
			// The `instr`, `local_stack` and `frame` values should be restored from the previous
			// call stack frame.
			frame--;  // This is now the caller's frame.

			bool non_void = frame->called_func_ret_size > 0;
			while (!callee_frame->block_stack.empty()) {
				auto block = callee_frame->block_stack.back();

				// We're returning from a non-void function, so the last block on the stack is the
				// return value. It's being used by the caller so we don't free it.
				if (!non_void || callee_frame->block_stack.size() != 1)
					thread.process_memory.freeBlock(block);

				callee_frame->block_stack.pop_back();
			}
			callee_frame->resetFrameData();

			// Load previous frame
			instr                       = frame->instr;  // This is already a pointer to next instr
			local_stack                 = frame->local_stack;
			frame->called_func_arg_size = 0;
			frame->called_func_ret_size = 0;
		}
		// Here the argument is `0` because of the convention defined in the op_call_func.
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(init_lany_type)(FUNCTION_ARGS) {
		{ performInit(instr, local_stack, frame, thread, TypeID(instr->arg1)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(deinit)(FUNCTION_ARGS) {
		{
			auto block = frame->block_stack.back();
			auto type  = thread.process_memory.getBlockType(block);
			frame->block_stack.pop_back();

			// @note: Removing block_id fo local_offset mappings is not needed here, since new inits
			// will overwrite the old mappings

			thread.process_memory.freeBlock(block);
			frame->local_stack_head -= type->getSize();
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(input_l64)(FUNCTION_ARGS) {
		{
			thread.setProcessStatus(api::WaitingForInput{});
			derefStack<i64>(local_stack, instr->arg0)
				= thread.process.getIO().getInput<i64>(thread);
			thread.setProcessStatus(api::Running{});
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(output_l64)(FUNCTION_ARGS) {
		{ thread.process.getIO().writeOutput(derefStack<u64>(local_stack, instr->arg0)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(input_l32)(FUNCTION_ARGS) {
		{
			thread.setProcessStatus(api::WaitingForInput{});
			derefStack<i64>(local_stack, instr->arg0)
				= thread.process.getIO().getInput<i32>(thread);
			thread.setProcessStatus(api::Running{});
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(output_l32)(FUNCTION_ARGS) {
		{ thread.process.getIO().writeOutput(derefStack<u32>(local_stack, instr->arg0)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(nop)(FUNCTION_ARGS) { FUNCTION_CONT(1); }

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_l64)(FUNCTION_ARGS) {
		CORE_PANIC("ext_l64 not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_type)(FUNCTION_ARGS) {
		CORE_PANIC("ext_type not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_field)(FUNCTION_ARGS) {
		CORE_PANIC("ext_field not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_type_field)(FUNCTION_ARGS) {
		CORE_PANIC("ext_type_field not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_type_l64)(FUNCTION_ARGS) {
		CORE_PANIC("ext_type_l64 not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(alloc_lptr_type)(FUNCTION_ARGS) {
		{
			auto type
				= thread.executing_program->types->at(vm::TypeID(static_cast<u32>(instr->arg1)));
			auto  block       = thread.process_memory.allocateHeap(type);
			auto& dst_pointer = derefStack<Pointer>(local_stack, instr->arg0);
			thread.process_memory.setPointer(dst_pointer, { block, 0 });
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(free_lptr)(FUNCTION_ARGS) {
		{
			thread.process_memory.freeBlock(derefStack<Pointer>(local_stack, instr->arg0).getBlock()
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ref_lptr_lany)(FUNCTION_ARGS) {
		{
			auto& pointer   = derefStack<Pointer>(local_stack, instr->arg0);
			auto  block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg1)];
			auto  block     = frame->block_stack[block_idx];
			thread.process_memory.setPointer(pointer, { block, 0 });
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lptr_lptr)(FUNCTION_ARGS) {
		{
			auto& dst = derefStack<Pointer>(local_stack, instr->arg0);
			auto  src = derefStack<Pointer>(local_stack, instr->arg1);
			thread.process_memory.setPointer(dst, src);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(setVTable_lptr_type)(FUNCTION_ARGS) {
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg0);
			auto type
				= thread.executing_program->types->at(TypeID(base::safeIntConv<usize>(instr->arg1)));

			// Objects are guaranteed to hold vtable pointer as their first field
			// by static verification.
			auto vt_pointer = reinterpret_cast<const Type**>(
				thread.process_memory.getPointerData(pointer, sizeof(Type*)).getBegin()
			);
			*vt_pointer = type.get();
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantSetInner_lvnt_type)(FUNCTION_ARGS) {
		{
			auto variant_block_index = frame->local_offset_to_block_idx[u64(instr->arg0)];
			auto variant_block       = frame->block_stack[variant_block_index];
			thread.process_memory.setNestedViewBlock(
				Pointer(variant_block, 0),
				thread.executing_program->types->at(TypeID(u64(instr->arg1)))
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantGetInner_lptr_lvnt)(FUNCTION_ARGS) {
		{
			auto& destination_pointer = derefStack<Pointer>(local_stack, instr->arg0);
			auto  variant_block_index = frame->local_offset_to_block_idx[u64(instr->arg1)];
			auto  parent_block        = frame->block_stack[variant_block_index];
			auto  wanted_type
				= thread.executing_program->types->at(vm::TypeID(static_cast<usize>(instr[1].arg0)));

			auto view_block_ref
				= thread.process_memory.getNestedViewBlock(Pointer(parent_block, 0), wanted_type);

			match_optional(view_block_ref.toOpt()) {
				opt_some(view_block) {
					thread.process_memory.setPointer(
						destination_pointer, thread.process_memory.newBlockReference(view_block, 0)
					);
				}
				opt_none { thread.process_memory.setPointer(destination_pointer, Pointer::null()); }
			}
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantSetInner_lptr_type)(FUNCTION_ARGS) {
		{
			auto variant_pointer = derefStack<Pointer>(local_stack, instr->arg0);
			thread.process_memory.setNestedViewBlock(
				variant_pointer, thread.executing_program->types->at(TypeID(u64(instr->arg1)))
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantGetInner_lptr_lptr)(FUNCTION_ARGS) {
		{
			auto& destination_pointer = derefStack<Pointer>(local_stack, instr->arg0);
			auto  variant_pointer     = derefStack<Pointer>(local_stack, instr->arg1);
			auto  wanted_type
				= thread.executing_program->types->at(vm::TypeID(static_cast<usize>(instr[1].arg0)));

			auto view_block_ref
				= thread.process_memory.getNestedViewBlock(variant_pointer, wanted_type);

			match_optional(view_block_ref.toOpt()) {
				opt_some(view_block) {
					thread.process_memory.setPointer(
						destination_pointer, Memory::newBlockReference(view_block, 0)
					);
				}
				opt_none { thread.process_memory.setPointer(destination_pointer, Pointer::null()); }
			}
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(upcast_lptr_lptr)(FUNCTION_ARGS) {
		{
			// Same as move_lptr_lptr, treated differently by static analysis.
			auto& dst = derefStack<Pointer>(local_stack, instr->arg0);
			auto  src = derefStack<Pointer>(local_stack, instr->arg1);
			thread.process_memory.setPointer(dst, src);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(downcast_lptr_lptr)(FUNCTION_ARGS) {
		{
			auto& dst = derefStack<Pointer>(local_stack, instr->arg0);
			auto  src = derefStack<Pointer>(local_stack, instr->arg1);

			auto dst_type
				= thread.executing_program->types->at(vm::TypeID(static_cast<usize>(instr[1].arg0)));

			// Classes are guaranteed to hold vtable pointer as their first field.
			TypeCRef real_src_type = *reinterpret_cast<const Type**>(
				thread.process_memory.getPointerData(src, sizeof(Type*)).getBegin()
			);
			auto cast_allowed = real_src_type->inheritsFrom(dst_type);

			thread.process_memory.setPointer(dst, cast_allowed ? src : Pointer::null());
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(store_lptr_lany)(FUNCTION_ARGS) {
		{
			auto dst_pointer = derefStack<Pointer>(local_stack, instr->arg0);

			auto src_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg1)];
			auto src_block     = frame->block_stack[src_block_idx];
			auto src_pointer   = Pointer(src_block, 0);

			auto type = Memory::getBlockType(src_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(load_lany_lptr)(FUNCTION_ARGS) {
		{
			auto dst_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg0)];
			auto dst_block     = frame->block_stack[dst_block_idx];
			auto dst_pointer   = Pointer(dst_block, 0);

			auto src_pointer = derefStack<Pointer>(local_stack, instr->arg1);

			auto type = Memory::getBlockType(dst_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLea_lptr_lptr)(FUNCTION_ARGS) {
		{
			auto& dst    = derefStack<Pointer>(local_stack, instr->arg0);
			auto  src    = derefStack<Pointer>(local_stack, instr->arg1);
			auto  offset = static_cast<usize>(instr[1].arg0);

			thread.process_memory.setPointer(dst, { src.getBlock(), offset });
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structStore_lptr_lany)(FUNCTION_ARGS) {
		{
			auto dst_pointer  = derefStack<Pointer>(local_stack, instr->arg0);
			auto field_offset = safeReadBytes<i64>(instr[1].arg0);
			dst_pointer.movePointer(field_offset);

			auto src_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg1)];
			auto src_block     = frame->block_stack[src_block_idx];
			auto src_pointer   = Pointer(src_block, 0);

			auto type = Memory::getBlockType(src_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLoad_lany_lptr)(FUNCTION_ARGS) {
		{
			auto dst_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg0)];
			auto dst_block     = frame->block_stack[dst_block_idx];
			auto dst_pointer   = Pointer(dst_block, 0);

			auto src_pointer  = derefStack<Pointer>(local_stack, instr->arg1);
			auto field_offset = safeReadBytes<i64>(instr[1].arg0);
			src_pointer.movePointer(field_offset);

			auto type = Memory::getBlockType(dst_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableLea_lptr_lptr)(FUNCTION_ARGS) {
		{
			auto& dst         = derefStack<Pointer>(local_stack, instr->arg0);
			auto  tbl_pointer = derefStack<Pointer>(local_stack, instr->arg1);
			auto  element_type
				= *thread.process_memory.getBlockType(tbl_pointer.getBlock())->getInnerType();
			auto index       = derefStack<i64>(local_stack, instr[1].arg0);
			auto data_offset = usize(index * i64(element_type->getSize()));

			thread.process_memory.setPointer(dst, { tbl_pointer.getBlock(), data_offset });
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableStore_lptr_lany)(FUNCTION_ARGS) {
		{
			auto tbl_pointer = derefStack<Pointer>(local_stack, instr->arg0);
			auto element_type
				= *thread.process_memory.getBlockType(tbl_pointer.getBlock())->getInnerType();
			auto index       = derefStack<i64>(local_stack, instr[1].arg0);
			auto data_offset = index * i64(element_type->getSize());
			tbl_pointer.movePointer(data_offset);

			auto src_block_idx = frame->local_offset_to_block_idx[instr->arg1];
			auto src_block     = frame->block_stack[src_block_idx];
			auto src_pointer   = Pointer(src_block, 0);

			thread.process_memory.copyPointedData(tbl_pointer, src_pointer, element_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableLoad_lany_lptr)(FUNCTION_ARGS) {
		{
			auto dst_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg0)];
			auto dst_block     = frame->block_stack[dst_block_idx];
			auto dst_pointer   = Pointer(dst_block, 0);

			auto tbl_pointer = derefStack<Pointer>(local_stack, instr->arg1);

			auto index = derefStack<i64>(local_stack, instr[1].arg0);
			auto element_type
				= *thread.process_memory.getBlockType(tbl_pointer.getBlock())->getInnerType();
			tbl_pointer.movePointer(index * i64(element_type->getSize()));

			thread.process_memory.copyPointedData(dst_pointer, tbl_pointer, element_type);
		}
		FUNCTION_CONT(2);
	}

	// `cast_lN_type` instructions are no-ops at runtime, they are only used by the validator.
#define CAST_PRIMITIVE(SIZE) \
	RETURN_TYPE OpFuns::OPCODE_NAME(cast_l##SIZE##_type)(FUNCTION_ARGS) { FUNCTION_CONT(1); }

	FOR_EACH(CAST_PRIMITIVE, 8, 16, 32, 64)
#undef CAST_PRIMITIVE

	RETURN_TYPE OpFuns::OPCODE_NAME(breakpoint)(FUNCTION_ARGS) {
		{
			instr += 1;
			save_execution_state(instr, local_stack, frame, thread);

			thread.handleBreakpoint();

			// Restore current flow.
			// They can be changed when doing "step by step" execution.
			frame       = thread.runtime_data.frame_stack_current;
			instr       = frame->instr;
			local_stack = frame->local_stack;
		}
		FUNCTION_CONT(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(label)(FUNCTION_ARGS) {
		CORE_PANIC("Handling label should not be possible");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(initFromVmValue)(FUNCTION_ARGS) {
		{
			const VmValue& vm_value = *std::bit_cast<const VmValue*>(instr->arg0);
			performInit(instr, local_stack, frame, thread, vm_value.type->getID());
			vm_value.exportData({ frame->block_stack.back(), 0 });
		}
		FUNCTION_CONT(1);
	}
}

#undef OPCODE_NAME
#undef FUNCTION_ARGS
#undef FUNCTION_CONT
#undef FUNCTION_CONT_CHECK_STRATEGY
