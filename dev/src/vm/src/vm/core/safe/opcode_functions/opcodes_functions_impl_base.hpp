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

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>
#include <base/preproc/for_each.hpp>
#include <base/types/ints.hpp>

#include <cstring>
#ifdef ENABLE_JIT
	#include <vm/core/jit/jit_compiler.hpp>
#endif
#include <base/types/floats.hpp>

#include <vm/core/builtin_functions.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/vmvalue/vmvalue.hpp>
#include <vm/utils/interpret.hpp>

#include <cmath>
#include <limits>

// jitable_interface.py depends on the instructions exact, fully-qualified names
#ifdef DEBUG_OPCODES
	#define OPCODE_NAME(name)   op_debug_##name
	#define FUNCTION_ARGS       OPFUN_REF_ARGS
	#define FUNCTION_CONT(step) instr += step;
	#define OP_FUN              vm::DebugOpFun
#else
	#define OPCODE_NAME(name)   op_##name
	#define FUNCTION_ARGS       OPFUN_ARGS
	#define FUNCTION_CONT(step) OPFUN_CONT(step)
	#define OP_FUN              vm::OpFun
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

	// `op_exit` is the only opcode without the `FUNCTION_CONT` macro. This means,
	// every other instruction will jump to the next at the end of it with
	// `FUNCTION_CONT`, so the the only way to end execution is to
	// use this opcode. It also requires different macro surrounding the function call in the
	// switch case because in this approach we can't end execution from
	// within the function, but we have to add some instructions on the outside of it. Hence we use
	// the `OP_CASE_END` macro that adds `goto End` instruction, residing after opcode function,
	// inside interpreter loop.
	RETURN_TYPE OpFuns::OPCODE_NAME(exit)(FUNCTION_ARGS) { IF_TC(return;) }

	RETURN_TYPE OpFuns::OPCODE_NAME(check_strategy)(FUNCTION_ARGS) {
		{
			++instr;
			if (thread.getExecutionRequestPendingFlag())
				return handle_execution_break(instr, local_stack, frame, thread);
		}
		FUNCTION_CONT(0);
	}

#define DEFINE_MOVE_OPS(BITS_SIZE, TYPE)                                                       \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_p##BITS_SIZE##_imm)(FUNCTION_ARGS) {                   \
		{ WRITE_TO_PLACE_ARG(TYPE, instr->arg0, safeReadObjectBytes<TYPE>(instr->arg1)); }     \
		FUNCTION_CONT(1);                                                                      \
	}                                                                                          \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_p##BITS_SIZE##_p##BITS_SIZE)(FUNCTION_ARGS) {          \
		{ WRITE_TO_PLACE_ARG(TYPE, instr->arg0, READ_FROM_PLACE_ARG(TYPE, instr->arg1)); }     \
		FUNCTION_CONT(1);                                                                      \
	}                                                                                          \
	RETURN_TYPE OpFuns::OPCODE_NAME(cmov_p##BITS_SIZE##_p##BITS_SIZE)(FUNCTION_ARGS) {         \
		{                                                                                      \
			if (frame->flags.flag) {                                                           \
				const auto value = READ_FROM_PLACE_ARG(TYPE, instr->arg1);                     \
				WRITE_TO_PLACE_ARG(TYPE, instr->arg0, value);                                  \
			}                                                                                  \
		}                                                                                      \
		FUNCTION_CONT(1);                                                                      \
	}                                                                                          \
	RETURN_TYPE OpFuns::OPCODE_NAME(cmov_p##BITS_SIZE##_imm)(FUNCTION_ARGS) {                  \
		{                                                                                      \
			if (frame->flags.flag)                                                             \
				WRITE_TO_PLACE_ARG(TYPE, instr->arg0, safeReadObjectBytes<TYPE>(instr->arg1)); \
		}                                                                                      \
		FUNCTION_CONT(1);                                                                      \
	}

	DEFINE_MOVE_OPS(64, u64)
	DEFINE_MOVE_OPS(32, u32)
	DEFINE_MOVE_OPS(16, u16)
	DEFINE_MOVE_OPS(8, u8)

#define DEFINE_ARITHMETIC_OP(NAME, BITS_SIZE, TYPE, OP)                                  \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##BITS_SIZE##_p##BITS_SIZE)(FUNCTION_ARGS) { \
		{                                                                                \
			auto       lhs = READ_FROM_PLACE_ARG(TYPE, instr->arg0);                     \
			const auto rhs = READ_FROM_PLACE_ARG(TYPE, instr->arg1);                     \
			lhs OP     rhs;                                                              \
			WRITE_TO_PLACE_ARG(TYPE, instr->arg0, lhs);                                  \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}                                                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##BITS_SIZE##_imm)(FUNCTION_ARGS) {          \
		{                                                                                \
			auto       lhs = READ_FROM_PLACE_ARG(TYPE, instr->arg0);                     \
			const auto rhs = READ_FROM_DIRECT_ARG(TYPE, instr->arg1);                    \
			lhs        OP static_cast<TYPE>(rhs);                                        \
			WRITE_TO_PLACE_ARG(TYPE, instr->arg0, lhs);                                  \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}

#define DEFINE_DIVISION_LIKE_OP(NAME, BITS_SIZE, TYPE, OP)                                \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##BITS_SIZE##_p##BITS_SIZE)(FUNCTION_ARGS) {  \
		{                                                                                 \
			auto       lhs = READ_FROM_PLACE_ARG(TYPE, instr->arg0);                      \
			const auto rhs = READ_FROM_PLACE_ARG(TYPE, instr->arg1);                      \
			if (rhs == static_cast<TYPE>(0)) throw exceptions::VMZeroDivisionException(); \
			lhs = static_cast<TYPE>(lhs OP rhs);                                          \
			WRITE_TO_PLACE_ARG(TYPE, instr->arg0, lhs);                                   \
		}                                                                                 \
		FUNCTION_CONT(1);                                                                 \
	}                                                                                     \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##BITS_SIZE##_imm)(FUNCTION_ARGS) {           \
		{                                                                                 \
			auto lhs = READ_FROM_PLACE_ARG(TYPE, instr->arg0);                            \
			auto rhs = READ_FROM_DIRECT_ARG(TYPE, instr->arg1);                           \
			if (rhs == static_cast<TYPE>(0)) throw exceptions::VMZeroDivisionException(); \
			lhs = static_cast<TYPE>(lhs OP rhs);                                          \
			WRITE_TO_PLACE_ARG(TYPE, instr->arg0, lhs);                                   \
		}                                                                                 \
		FUNCTION_CONT(1);                                                                 \
	}

#define DEFINE_NEGATION_OP(NAME, BITS_SIZE, TYPE)                                \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##BITS_SIZE)(FUNCTION_ARGS) {        \
		{                                                                        \
			auto value = READ_FROM_PLACE_ARG(TYPE, instr->arg0);                 \
			WRITE_TO_PLACE_ARG(TYPE, instr->arg0, value* static_cast<TYPE>(-1)); \
		}                                                                        \
		FUNCTION_CONT(1);                                                        \
	}

// @TODO: #1216 Check for over/under flows.
#define DEFINE_INT_N_ARITHMETIC(SIZE)               \
	DEFINE_ARITHMETIC_OP(add, SIZE, i##SIZE, +=)    \
	DEFINE_ARITHMETIC_OP(sub, SIZE, i##SIZE, -=)    \
	DEFINE_ARITHMETIC_OP(mul, SIZE, i##SIZE, *=)    \
	DEFINE_DIVISION_LIKE_OP(mod, SIZE, i##SIZE, %)  \
	DEFINE_DIVISION_LIKE_OP(div, SIZE, i##SIZE, /)  \
	DEFINE_NEGATION_OP(neg, SIZE, i##SIZE)          \
	DEFINE_ARITHMETIC_OP(umul, SIZE, u##SIZE, *=)   \
	DEFINE_DIVISION_LIKE_OP(umod, SIZE, u##SIZE, %) \
	DEFINE_DIVISION_LIKE_OP(udiv, SIZE, u##SIZE, /)

	FOR_EACH(DEFINE_INT_N_ARITHMETIC, 64, 32, 16, 8)

#define FLOAT_64_TYPE f64
#define FLOAT_32_TYPE f32
#define DEFINE_FLOAT_N_ARITHMETIC(SIZE)                         \
	DEFINE_ARITHMETIC_OP(fadd, SIZE, FLOAT_##SIZE##_TYPE, +=)   \
	DEFINE_ARITHMETIC_OP(fsub, SIZE, FLOAT_##SIZE##_TYPE, -=)   \
	DEFINE_ARITHMETIC_OP(fmul, SIZE, FLOAT_##SIZE##_TYPE, *=)   \
	DEFINE_DIVISION_LIKE_OP(fdiv, SIZE, FLOAT_##SIZE##_TYPE, /) \
	DEFINE_NEGATION_OP(fneg, SIZE, FLOAT_##SIZE##_TYPE)

	FOR_EACH(DEFINE_FLOAT_N_ARITHMETIC, 64, 32)


#define DEFINE_BOOLEAN_OP(NAME, OP)                                              \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p8##_p8)(FUNCTION_ARGS) {             \
		{                                                                        \
			const bool lhs = (READ_FROM_PLACE_ARG(u8, instr->arg0) != u8{ 0 });  \
			const bool rhs = (READ_FROM_PLACE_ARG(u8, instr->arg1) != u8{ 0 });  \
			WRITE_TO_PLACE_ARG(u8, instr->arg0, static_cast<u8>(lhs OP rhs));    \
		}                                                                        \
		FUNCTION_CONT(1);                                                        \
	}                                                                            \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p8##_imm)(FUNCTION_ARGS) {            \
		{                                                                        \
			const bool lhs = (READ_FROM_PLACE_ARG(u8, instr->arg0) != u8{ 0 });  \
			const bool rhs = (READ_FROM_DIRECT_ARG(u8, instr->arg1) != u8{ 0 }); \
			WRITE_TO_PLACE_ARG(u8, instr->arg0, static_cast<u8>(lhs OP rhs));    \
		}                                                                        \
		FUNCTION_CONT(1);                                                        \
	}

	DEFINE_BOOLEAN_OP(log_and, &&)
	DEFINE_BOOLEAN_OP(log_or, ||)
	DEFINE_BOOLEAN_OP(log_xor, !=)

	RETURN_TYPE OpFuns::OPCODE_NAME(log_not_p8)(FUNCTION_ARGS) {
		{
			bool result = (READ_FROM_PLACE_ARG(u8, instr->arg0) == u8{ 0 });
			WRITE_TO_PLACE_ARG(u8, instr->arg0, (result ? u8{ 1 } : u8{ 0 }));
		}
		FUNCTION_CONT(1);
	}

#define DEFINE_COMPARISON_OP(NAME, BITS_SIZE, TYPE, OP)                                  \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##BITS_SIZE##_p##BITS_SIZE)(FUNCTION_ARGS) { \
		{                                                                                \
			frame->flags.flag = READ_FROM_PLACE_ARG(TYPE, instr->arg0)                   \
				OP READ_FROM_PLACE_ARG(TYPE, instr->arg1);                               \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}                                                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##BITS_SIZE##_imm)(FUNCTION_ARGS) {          \
		{                                                                                \
			frame->flags.flag = READ_FROM_PLACE_ARG(TYPE, instr->arg0)                   \
				OP READ_FROM_DIRECT_ARG(TYPE, instr->arg1);                              \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}

#define DEFINE_INT_N_COMPARISONS(SIZE)              \
	DEFINE_COMPARISON_OP(cmpEq, SIZE, i##SIZE, ==)  \
	DEFINE_COMPARISON_OP(cmpNeq, SIZE, i##SIZE, !=) \
	DEFINE_COMPARISON_OP(cmpGt, SIZE, i##SIZE, >)   \
	DEFINE_COMPARISON_OP(cmpGe, SIZE, i##SIZE, >=)  \
	DEFINE_COMPARISON_OP(cmpLt, SIZE, i##SIZE, <)   \
	DEFINE_COMPARISON_OP(cmpLe, SIZE, i##SIZE, <=)  \
	DEFINE_COMPARISON_OP(ucmpGt, SIZE, u##SIZE, >)  \
	DEFINE_COMPARISON_OP(ucmpGe, SIZE, u##SIZE, >=) \
	DEFINE_COMPARISON_OP(ucmpLt, SIZE, u##SIZE, <)  \
	DEFINE_COMPARISON_OP(ucmpLe, SIZE, u##SIZE, <=)

	FOR_EACH(DEFINE_INT_N_COMPARISONS, 64, 32, 16, 8)

#define DEFINE_FLOAT_N_COMPARISONS(SIZE)                         \
	DEFINE_COMPARISON_OP(fcmpEq, SIZE, FLOAT_##SIZE##_TYPE, ==)  \
	DEFINE_COMPARISON_OP(fcmpNeq, SIZE, FLOAT_##SIZE##_TYPE, !=) \
	DEFINE_COMPARISON_OP(fcmpGt, SIZE, FLOAT_##SIZE##_TYPE, >)   \
	DEFINE_COMPARISON_OP(fcmpGe, SIZE, FLOAT_##SIZE##_TYPE, >=)  \
	DEFINE_COMPARISON_OP(fcmpLt, SIZE, FLOAT_##SIZE##_TYPE, <)   \
	DEFINE_COMPARISON_OP(fcmpLe, SIZE, FLOAT_##SIZE##_TYPE, <=)

	FOR_EACH(DEFINE_FLOAT_N_COMPARISONS, 64, 32)

	RETURN_TYPE OpFuns::OPCODE_NAME(cmpNull_pptr)(FUNCTION_ARGS) {
		{
			auto pointer      = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			frame->flags.flag = pointer.isNull();
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(jmp_label)(FUNCTION_ARGS) {
		{ instr += instr->arg0; }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(jmpIf_label)(FUNCTION_ARGS) {
		{
			if (frame->flags.flag) instr += instr->arg0;
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(jmpIfNot_label)(FUNCTION_ARGS) {
		{
			if (!frame->flags.flag) instr += instr->arg0;
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(call_func)(FUNCTION_ARGS) {
		{
			auto function_id = static_cast<usize>(instr->arg0);
			CORE_ASSERT(
				SafeVMThread::isCallableFunctionID(function_id),
				"Start function should not be called in the runtime!"
			);
			performFunctionCall(instr, local_stack, frame, thread, function_id);
		}
		// After acquiring the `executing_code` of the new function we have instruction pointer
		// (`instr`) pointing at the first instruction of the new function, so moving forward by one
		// would mean that we skipped the first instruction. That's why we move forward zero
		// instructions. For future returns, the first instruction that should be executed after
		// call is saved on frame so that `op_ret`s have to move forward zero instructions after
		// restoring `instr` from frame.
		FUNCTION_CONT(0);
	}
#ifdef ENABLE_JIT
	RETURN_TYPE OpFuns::OPCODE_NAME(jitEntrypoint)(FUNCTION_ARGS) {
		{
			auto& jit_data         = thread.safe_process.getJitData();
			auto& current_func_obj = *frame->current_function;
			auto  current_func_id  = current_func_obj.id;
			auto  instr_offset     = instr - current_func_obj.bc.data();

			jit::JitFuncData& my_data = jit_data[current_func_id];
			auto& compiled_code_ptr = my_data.compiled_code_ptrs[instr_offset];
			auto& until_compilation = my_data.until_compilation[instr_offset];

			if (compiled_code_ptr) {
				// is already compiled
				const MicroInstruction* saved_instr = instr;
				const Frame*            saved_frame = frame;
				i64 offset = (*compiled_code_ptr)(&instr, &local_stack, &frame, &thread);
				if (saved_frame == frame) instr = saved_instr + offset;
			} else if (0 < until_compilation) {
				// should be compiled later
				--until_compilation;

			    save_execution_state(instr, local_stack, frame, thread);
				thread.executeOneStep();

                // Restore current flow.
                // They can be changed when doing "step by step" execution.
                frame       = thread.runtime_data.frame_stack_current;
                instr       = frame->instr;
                local_stack = frame->local_stack;
			} else {
				// should be compiled now
				const auto* program_copy
					= dynamic_cast<const low::LowVMProgramCopy*>(thread.process_program.get());
				CORE_ASSERT(program_copy, "Jit entrypoints should be only in LowVMProgramCopy.");
                auto original_function
					= program_copy->getOriginalProgram()->getFunctions()[current_func_id];

				MRef<jit::JitOpFun> compiled = jit::compileLLVM(
					my_data.cfgs[instr_offset], original_function.bc, current_func_obj.name
				);
				CORE_ASSERT(compiled, "Compiled function pointer shouldn't be nullptr");
				compiled_code_ptr = compiled;

				const MicroInstruction* saved_instr = instr;
				const Frame*            saved_frame = frame;
				i64 offset = (*compiled_code_ptr)(&instr, &local_stack, &frame, &thread);
				if (saved_frame == frame) instr = saved_instr + offset;
			}
		}
		FUNCTION_CONT(0);
	}
#endif

	RETURN_TYPE OpFuns::OPCODE_NAME(call_builtinfunc)(FUNCTION_ARGS) {
		{
			auto builtin_id         = static_cast<builtins::BuiltinFunctionID>(instr->arg0);
			auto function_signature = builtins::getBuiltinFunctionSignature(builtin_id);
			auto arg_count          = function_signature->parameters.size();

			std::vector<Box<VmValue>> args;
			auto                      block_ref_stack_count
				= usize(frame->local_block_ref_stack_end - frame->local_block_ref_stack_base);
			u64 first_arg_idx = block_ref_stack_count - arg_count;

			// Create VmValue objects from local arguments.
			for (u64 i = 0; i < arg_count; i++) {
				const base::StrID arg_type  = function_signature->parameters[i];
				TypeCRef          real_type = thread.process_program->getTypes().at(arg_type);
				auto              block = Ref(frame->local_block_ref_stack_base[first_arg_idx + i]);
				args.push_back(thread.safe_process.createOwnedVmValue(real_type, Pointer(block, 0)));
			}

			std::vector<TypeCRef> result_types = {};
			auto                  ret_count    = function_signature->result_types.size();
			result_types.reserve(ret_count);
			for (u64 i = 0; i < ret_count; i++) {
				result_types.emplace_back(
					thread.process_program->getTypes().at(function_signature->result_types[i])
				);
			}

			base::Optional<Box<VmValue>> return_value = builtins::callBuiltinFunction(
				builtin_id, result_types, thread.safe_process, thread, args
			);

			if (return_value.has_value()) {
				auto value = std::move(return_value.value());
				value->exportData(
					Pointer(Ref(frame->local_block_ref_stack_base[first_arg_idx - 1]), 0)
				);
				value->freeData();
			}
			for (auto& vm_value: args) vm_value->freeData();

			// Similar as in call_func, but we deinit the arguments blocks as well,
			// but without the return value.
			for (u64 i = 0; i < arg_count; i++) performDeinit(frame, thread);
		}

		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(call_cfunc)(FUNCTION_ARGS) {
		{
			auto ext_func = READ_FROM_DIRECT_ARG(CRef<low::LowExternCFunction>, instr->arg0);

			auto arg_count = ext_func->parameters.size();
			bool is_void   = ext_func->result_types.size() == 0;
			CORE_ASSERT(
				ext_func->result_types.size() <= 1, "C function cannot return more than 1 type"
			);

			if (arg_count == 0 && is_void) {
				// Special case: void function with no arguments.
				ext_func->function_pointer(nullptr, nullptr);
			} else {
				// Calculate the index of the result value on the block stack.
				// If the function is void, there is no result value, so we don't
				// need to account for it.
				// Local stack layout:
				// 		CURRENT_FUNC_RESULT_VALUE (this is where the stack begins)
				// 		...
				// 		result_value,
				// 		arg0,
				// 		arg1
				// 		...
				// 		argN
				u64 block_ref_stack_count
					= u64(frame->local_block_ref_stack_end - frame->local_block_ref_stack_base);
				u64 result_value_idx = block_ref_stack_count - arg_count - (is_void ? 0 : 1);


				auto ext_result_destination
					= Ref(frame->local_block_ref_stack_base[result_value_idx]);
				auto result_view = thread.process_memory.getBlockViewUnsafe(ext_result_destination);

				// Prepare arguments and call the function.
				byte* result_pointer = result_view.getBegin();
				byte* args_pointer
					= result_pointer
				    + (is_void ? 0 : ext_func->result_types.at(0)->getSize().asInt());

				ext_func->function_pointer(result_pointer, args_pointer);

				for (u64 i = 0; i < arg_count; i++) performDeinit(frame, thread);
			}
		}

		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(set_threadctx)(FUNCTION_ARGS) {
		{
			auto& called_func = thread.process_program->getFunctions()[instr->arg0];
			thread.setThreadCtx(called_func.name.str());
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(virtual_call_pptr_method)(FUNCTION_ARGS) {
		{
			const auto pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg0);

			// Objects are guaranteed to hold inheritance metadata pointers as their first field.
			// This is verified by static verification.

			const auto  view             = Memory::getPointerData(pointer, sizeof(Type*));
			const auto* inh_meta_pointer = readFromView<const Type*>(view);

			if (inh_meta_pointer == nullptr) throw exceptions::VMVtableUnset();

			const auto inh_metadata = inh_meta_pointer->getInheritanceMetadata().value();
			const auto method_name  = thread.process_program->getMethodNamePool()[instr->arg1];
			const auto implementation_name = inh_metadata->vtable[method_name];

			const usize function_id
				= *thread.process_program->getFunctions().idOf(implementation_name);

			CORE_ASSERT(
				SafeVMThread::isCallableFunctionID(function_id),
				"Start function should not be called in the runtime!"
			);

			performFunctionCall(instr, local_stack, frame, thread, function_id);
		}
		FUNCTION_CONT(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret_tailcall_func)(FUNCTION_ARGS) {
		{
			auto function_id = static_cast<usize>(instr->arg0);
			CORE_ASSERT(
				SafeVMThread::isCallableFunctionID(function_id),
				"Start function should not be called in the runtime!"
			);
			auto& function          = thread.process_program->getFunctions()[function_id];
			instr                   = function.bc.data();
			frame->current_function = &function;

			if (local_stack + function.local_stack_size > thread.runtime_data.local_stack_end)
				throw exceptions::VMStackOverflowException();
		}
		FUNCTION_CONT(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret)(FUNCTION_ARGS) {
		{
			// Frame of the function we're returning from.
			auto* callee_frame = frame;
			u64   ret_count    = frame->current_function->result_types.size();

			// We have to update values passed in arguments.
			// Old `instr` and `local_stack` are stored on the previous frame.
			// Previous frame is just before current frame in the array, so that
			// substracting one from the pointer will give us the previous frame.
			// The `instr`, `local_stack` and `frame` values should be restored from the previous
			// call stack frame.
			frame--;  // This is now the caller's frame.

			while (callee_frame->local_block_ref_stack_end
			       > callee_frame->local_block_ref_stack_base) {
				auto block           = Ref(callee_frame->local_block_ref_stack_end[-1]);
				u64  block_ref_count = u64(
                    callee_frame->local_block_ref_stack_end
                    - callee_frame->local_block_ref_stack_base
                );

				// We're returning from a non-void function, so the last `ret_count` blocks on the
				// stack are the return values. They are being used by the caller so we don't free them.
				if (block_ref_count > ret_count) {
					thread.process_memory.freeBlockData(block);
					thread.process_memory.decreaseBlockRefcount(block);
				}

				callee_frame->local_block_ref_stack_end--;
			}
			callee_frame->resetFrameData();

			// Load previous frame.
			instr       = frame->instr;  // This is already a pointer to next instr.
			local_stack = frame->local_stack;
		}
		// Here the argument is `0` because of the convention defined in the op_call_func.
		FUNCTION_CONT(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(init_bany_type)(FUNCTION_ARGS) {
		{
			performInit(
				instr, local_stack, frame, thread, READ_FROM_DIRECT_ARG(TypeCRef, instr->arg1)
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(deinit)(FUNCTION_ARGS) {
		{ performDeinit(frame, thread); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(input_p64)(FUNCTION_ARGS) {
		{
			thread.setProcessStatus(api::Sleeping{});
			i64 io_value = thread.safe_process.getIO().getInput<i64>(thread);
			WRITE_TO_PLACE_ARG(i64, instr->arg0, io_value);
			thread.setProcessStatus(api::Running{});
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(output_p64)(FUNCTION_ARGS) {
		{ thread.safe_process.getIO().writeOutput(READ_FROM_PLACE_ARG(u64, instr->arg0)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(input_p32)(FUNCTION_ARGS) {
		{
			thread.setProcessStatus(api::Sleeping{});
			i32 io_value = thread.safe_process.getIO().getInput<i32>(thread);
			WRITE_TO_PLACE_ARG(i32, instr->arg0, io_value);
			thread.setProcessStatus(api::Running{});
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(output_p32)(FUNCTION_ARGS) {
		{ thread.safe_process.getIO().writeOutput(READ_FROM_PLACE_ARG(u32, instr->arg0)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(strOutput_pptr)(FUNCTION_ARGS) {
		{
			auto ptr = READ_FROM_PLACE_ARG(Pointer, instr->arg0);

			auto block      = ptr.getBlock();
			auto block_id   = thread.process_memory.requestBlockID(block);
			auto block_data = thread.process_memory.requestBlockData(block_id);
			auto str_data   = block_data.stdString();
			thread.safe_process.getIO().writeOutput(str_data.substr(0, str_data.size() - 1));
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(nop)(FUNCTION_ARGS) { FUNCTION_CONT(1); }

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_p64)(FUNCTION_ARGS) {
		CORE_PANIC("ext_p64 not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_imm)(FUNCTION_ARGS) {
		CORE_PANIC("ext_imm not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_type)(FUNCTION_ARGS) {
		CORE_PANIC("ext_type not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_field)(FUNCTION_ARGS) {
		CORE_PANIC("ext_field not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_p64_type)(FUNCTION_ARGS) {
		CORE_PANIC("ext_p64_type not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_type_field)(FUNCTION_ARGS) {
		CORE_PANIC("ext_type_field not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_type_p64)(FUNCTION_ARGS) {
		CORE_PANIC("ext_type_p64 not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_type_type)(FUNCTION_ARGS) {
		CORE_PANIC("ext_type_type not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(alloc_pptr_type)(FUNCTION_ARGS) {
		{
			const auto dst     = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto       type    = READ_FROM_DIRECT_ARG(TypeCRef, instr->arg1);
			auto       block   = thread.process_memory.allocateHeap(type);
			const auto new_dst = thread.process_memory.updatePointerAssignment(dst, { block, 0 });
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(free_pptr)(FUNCTION_ARGS) {
		{
			if (auto ptr = READ_FROM_PLACE_ARG(Pointer, instr->arg0))
				thread.process_memory.freeBlockData(ptr.getBlock());
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ref_pptr_bany)(FUNCTION_ARGS) {
		{
			const auto dst     = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto       block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			const auto new_dst = thread.process_memory.updatePointerAssignment(dst, { block, 0 });
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_pptr_pptr)(FUNCTION_ARGS) {
		{
			const auto    dst     = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			const auto    src     = READ_FROM_PLACE_ARG(Pointer, instr->arg1);
			const Pointer new_dst = thread.process_memory.updatePointerAssignment(dst, src);
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_popq_popq)(FUNCTION_ARGS) {
		{
			const auto type_size = instr[1].arg0;
			auto       dst       = getBytePtrFromPlaceArg(
                local_stack, thread.runtime_data.global_data_buffer_base, instr->arg0
            );
			auto src = getBytePtrFromPlaceArg(
				local_stack, thread.runtime_data.global_data_buffer_base, instr->arg1
			);
			std::memcpy(dst, src, type_size);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_popq_imm)(FUNCTION_ARGS) {
		{
			// @TODO: #1728 remove this evil instruction
			void* value = READ_FROM_DIRECT_ARG(void*, instr->arg1);
			WRITE_TO_PLACE_ARG(void*, instr->arg0, value);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_bste_bste)(FUNCTION_ARGS) {
		{
			auto dst_block = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			auto src_block = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			thread.process_memory.copyPointedData(
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block)
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_bfst_bfst)(FUNCTION_ARGS) {
		{
			Ref<vm::Block> dst_block = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			Ref<vm::Block> src_block = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			thread.process_memory.copyPointedData(
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block)
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(setNull_pptr)(FUNCTION_ARGS) {
		{
			const auto    dst = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			const Pointer new_dst
				= thread.process_memory.updatePointerAssignment(dst, Pointer::null());
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(setVTable_pptr_type)(FUNCTION_ARGS) {
		{
			auto pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto type    = READ_FROM_DIRECT_ARG(TypeCRef, instr->arg1);

			// Objects hold vtable pointer as their first field.
			auto view = thread.process_memory.getPointerData(pointer, sizeof(Type*));
			// This writes a pointer to the type at object's first field.
			writeToView<const Type*>(view, type.get());
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(resetVTable_pptr)(FUNCTION_ARGS) {
		{
			auto pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto view    = thread.process_memory.getPointerData(pointer, sizeof(Type*));
			writeToView<const Type*>(view, nullptr);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantSetInner_bvnt_type)(FUNCTION_ARGS) {
		{
			auto variant_block = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			auto alt_type      = READ_FROM_DIRECT_ARG(TypeCRef, instr->arg1);
			auto variant_type  = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg0);
			OpFuns::setVariantType(thread, Pointer(variant_block, 0), alt_type, variant_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantGetInner_pptr_bvnt)(FUNCTION_ARGS) {
		{
			const auto dst           = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto       variant_block = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			auto       alt_type      = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg0);
			auto       variant_type  = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg1);

			const auto new_dst = thread.process_memory.updatePointerAssignment(
				dst, OpFuns::getVariantPtr(thread, Pointer(variant_block, 0), alt_type, variant_type)
			);
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantSetInner_pptr_type)(FUNCTION_ARGS) {
		{
			auto variant_pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto alt_type        = READ_FROM_DIRECT_ARG(TypeCRef, instr->arg1);
			auto variant_type    = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg0);
			OpFuns::setVariantType(thread, variant_pointer, alt_type, variant_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantGetInner_pptr_pptr)(FUNCTION_ARGS) {
		{
			const auto dst             = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto       variant_pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg1);
			auto       alt_type        = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg0);
			auto       variant_type    = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg1);

			const auto new_dst = thread.process_memory.updatePointerAssignment(
				dst, OpFuns::getVariantPtr(thread, variant_pointer, alt_type, variant_type)
			);
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}

		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(upcast_pptr_pptr)(FUNCTION_ARGS) {
		{
			// Same as move_pptr_pptr, treated differently by static analysis.
			const auto    dst     = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			const auto    src     = READ_FROM_PLACE_ARG(Pointer, instr->arg1);
			const Pointer new_dst = thread.process_memory.updatePointerAssignment(dst, src);
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(downcast_pptr_pptr)(FUNCTION_ARGS) {
		{
			const auto dst = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			const auto src = READ_FROM_PLACE_ARG(Pointer, instr->arg1);

			auto dst_type = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg0);

			// Classes are guaranteed to hold vtable pointer as their first field.
			auto        view         = thread.process_memory.getPointerData(src, sizeof(Type*));
			const auto* src_ptr      = readFromView<const Type*>(view);
			auto        cast_allowed = src_ptr->inheritsFrom(dst_type);

			const Pointer new_dst = thread.process_memory.updatePointerAssignment(
				dst, cast_allowed ? src : Pointer::null()
			);
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(store_pptr_bany)(FUNCTION_ARGS) {
		{
			auto dst_pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto src_block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			auto src_pointer = Pointer(src_block, 0);

			auto type = Memory::getBlockType(src_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(load_bany_pptr)(FUNCTION_ARGS) {
		{
			auto dst_block   = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			auto dst_pointer = Pointer(dst_block, 0);

			auto src_pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg1);

			auto type = Memory::getBlockType(dst_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLea_pptr_pptr)(FUNCTION_ARGS) {
		{
			const auto dst    = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto       src    = READ_FROM_PLACE_ARG(Pointer, instr->arg1);
			auto       offset = static_cast<usize>(instr[1].arg0);

			const Pointer new_dst = thread.process_memory.updatePointerAssignment(
				dst, { src.getBlock(), src.getOffset() + offset }
			);
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structStore_pptr_bany)(FUNCTION_ARGS) {
		{
			auto dst_pointer  = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto field_offset = READ_FROM_DIRECT_ARG(u64, instr[1].arg0);
			dst_pointer.movePointer(field_offset);

			auto src_block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			auto src_pointer = Pointer(src_block, 0);

			auto type = Memory::getBlockType(src_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLoad_bany_pptr)(FUNCTION_ARGS) {
		{
			auto dst_block   = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			auto dst_pointer = Pointer(dst_block, 0);

			auto src_pointer  = READ_FROM_PLACE_ARG(Pointer, instr->arg1);
			auto field_offset = READ_FROM_DIRECT_ARG(u64, instr[1].arg0);
			src_pointer.movePointer(field_offset);

			auto type = Memory::getBlockType(dst_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLea_pptr_bste)(FUNCTION_ARGS) {
		{
			const auto dst       = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			const auto src_block = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			auto       offset    = READ_FROM_DIRECT_ARG(u64, instr[1].arg0);

			const Pointer new_dst
				= thread.process_memory.updatePointerAssignment(dst, { src_block, offset });
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structStore_bste_bany)(FUNCTION_ARGS) {
		{
			auto dst_block   = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			auto dst_pointer = Pointer(dst_block, 0);

			auto field_offset = READ_FROM_DIRECT_ARG(u64, instr[1].arg0);
			dst_pointer.movePointer(field_offset);

			auto src_block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			auto src_pointer = Pointer(src_block, 0);

			auto type = Memory::getBlockType(src_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLoad_bany_bste)(FUNCTION_ARGS) {
		{
			auto dst_block   = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			auto dst_pointer = Pointer(dst_block, 0);

			auto src_block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			auto src_pointer = Pointer(src_block, 0);

			auto field_offset = READ_FROM_DIRECT_ARG(u64, instr[1].arg0);
			src_pointer.movePointer(field_offset);

			auto type = Memory::getBlockType(dst_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(anyArrayLea_pptr_pptr)(FUNCTION_ARGS) {
		{
			auto dst         = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto tbl_pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg1);
			u64  index       = READ_FROM_PLACE_ARG(u64, instr[1].arg0);
			auto elem_type   = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg1);
			u64  data_offset = elem_type->getSize().asInt() * index;

			const Pointer new_dst = thread.process_memory.updatePointerAssignment(
				dst, { tbl_pointer.getBlock(), tbl_pointer.getOffset() + data_offset }
			);
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(anyArrayStore_pptr_bany)(FUNCTION_ARGS) {
		{
			auto tbl_pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			u64  index       = READ_FROM_PLACE_ARG(u64, instr[1].arg0);
			auto elem_type   = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg1);
			u64  data_offset = index * elem_type->getSize().asInt();

			tbl_pointer.movePointer(data_offset);

			Ref<Block> src_block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			auto       src_pointer = Pointer(src_block, 0);

			thread.process_memory.copyPointedData(tbl_pointer, src_pointer, elem_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(anyArrayLoad_bany_pptr)(FUNCTION_ARGS) {
		{
			Ref<Block> dst_block   = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			Pointer    dst_pointer = Pointer(dst_block, 0);
			auto       tbl_pointer = READ_FROM_PLACE_ARG(Pointer, instr->arg1);
			u64        index       = READ_FROM_PLACE_ARG(u64, instr[1].arg0);
			auto       elem_type   = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg1);
			u64        data_offset = index * elem_type->getSize().asInt();

			tbl_pointer.movePointer(data_offset);

			thread.process_memory.copyPointedData(dst_pointer, tbl_pointer, elem_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableLea_pptr_bfst)(FUNCTION_ARGS) {
		{
			const auto dst         = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			Ref<Block> tbl_block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			u64        index       = READ_FROM_PLACE_ARG(u64, instr[1].arg0);
			auto       elem_type   = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg1);
			u64        data_offset = index * elem_type->getSize().asInt();

			const Pointer new_dst
				= thread.process_memory.updatePointerAssignment(dst, { tbl_block, data_offset });
			WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableLoad_bany_bfst)(FUNCTION_ARGS) {
		{
			Ref<Block> dst_block   = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			auto       dst_pointer = Pointer(dst_block, 0);
			Ref<Block> tbl_block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			u64        index       = READ_FROM_PLACE_ARG(u64, instr[1].arg0);
			auto       elem_type   = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg1);
			u64        data_offset = index * elem_type->getSize().asInt();

			auto src_pointer = Pointer(tbl_block, data_offset);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, elem_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableStore_bfst_bany)(FUNCTION_ARGS) {
		{
			Ref<Block> dst_block   = READ_BLOCK_REF_FROM_ARG(instr->arg0);
			Ref<Block> src_block   = READ_BLOCK_REF_FROM_ARG(instr->arg1);
			u64        index       = READ_FROM_PLACE_ARG(u64, instr[1].arg0);
			auto       elem_type   = READ_FROM_DIRECT_ARG(TypeCRef, instr[1].arg1);
			u64        data_offset = index * elem_type->getSize().asInt();

			auto dst_pointer = Pointer(dst_block, data_offset);
			auto src_pointer = Pointer(src_block, 0);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, elem_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(dynTableReAlloc_pptr_type)(FUNCTION_ARGS) {
		{
			auto tbl_pointer    = READ_FROM_PLACE_ARG(Pointer, instr->arg0);
			auto pointed_type   = READ_FROM_DIRECT_ARG(TypeCRef, instr->arg1);
			auto new_elem_count = READ_FROM_PLACE_ARG(u64, instr[1].arg0);

			if (new_elem_count == 0) {
				// When reallocating dynamic data to 0 elements, we free the data and set pointer to
				// null. This is one of two possible approaches:
				// 1. Current approach: treat 0-sized arrays as non-existent, and set the pointer to
				// null-pointer (what we do here)
				// 2. Alternative approach: Simply allow blocks of size 0 -- they would keep the
				// C-nullptr as their data, but on DVM level we would still allow pointer
				// [0-sized-block, nullptr] to exist. Any access to such block would simply
				// be out-of-bound access.
				//
				// It might be desired to switch to second approach in the future, depending on the
				// semantics of Duckling arrays.
				if (!tbl_pointer.isNull()) {
					thread.process_memory.freeBlockData(tbl_pointer.getBlock());
					const Pointer new_dst = thread.process_memory.updatePointerAssignment(
						tbl_pointer, Pointer::null()
					);
					WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
				}
			} else if (tbl_pointer.isNull()) {
				auto new_block
					= thread.process_memory.dynTableAllocateHeapN(pointed_type, new_elem_count);
				const Pointer new_dst
					= thread.process_memory.updatePointerAssignment(tbl_pointer, { new_block, 0 });
				WRITE_TO_PLACE_ARG(Pointer, instr->arg0, new_dst);
			} else {
				auto tbl_block = tbl_pointer.getBlock();
				thread.process_memory.dynTableReallocateBlockDataN(tbl_block, new_elem_count);
			}
		}
		FUNCTION_CONT(2);
	}

#define DEFINE_STATIC_CAST_CONVERSION_OP(NAME, DST_SIZE, SRC_SIZE, DST_TYPE, SRC_TYPE) \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##DST_SIZE##_p##SRC_SIZE)(FUNCTION_ARGS) { \
		{                                                                              \
			auto val = READ_FROM_PLACE_ARG(SRC_TYPE, instr->arg1);                     \
			WRITE_TO_PLACE_ARG(DST_TYPE, instr->arg0, static_cast<DST_TYPE>(val));     \
		}                                                                              \
		FUNCTION_CONT(1);                                                              \
	}

	// Sign Extension
	DEFINE_STATIC_CAST_CONVERSION_OP(sext, 16, 8, i16, i8)
	DEFINE_STATIC_CAST_CONVERSION_OP(sext, 32, 8, i32, i8)
	DEFINE_STATIC_CAST_CONVERSION_OP(sext, 64, 8, i64, i8)
	DEFINE_STATIC_CAST_CONVERSION_OP(sext, 32, 16, i32, i16)
	DEFINE_STATIC_CAST_CONVERSION_OP(sext, 64, 16, i64, i16)
	DEFINE_STATIC_CAST_CONVERSION_OP(sext, 64, 32, i64, i32)

	// Zero Extension
	DEFINE_STATIC_CAST_CONVERSION_OP(zext, 16, 8, u16, u8)
	DEFINE_STATIC_CAST_CONVERSION_OP(zext, 32, 8, u32, u8)
	DEFINE_STATIC_CAST_CONVERSION_OP(zext, 64, 8, u64, u8)
	DEFINE_STATIC_CAST_CONVERSION_OP(zext, 32, 16, u32, u16)
	DEFINE_STATIC_CAST_CONVERSION_OP(zext, 64, 16, u64, u16)
	DEFINE_STATIC_CAST_CONVERSION_OP(zext, 64, 32, u64, u32)

	// Truncation
	DEFINE_STATIC_CAST_CONVERSION_OP(trunc, 8, 16, u8, u16)
	DEFINE_STATIC_CAST_CONVERSION_OP(trunc, 8, 32, u8, u32)
	DEFINE_STATIC_CAST_CONVERSION_OP(trunc, 8, 64, u8, u64)
	DEFINE_STATIC_CAST_CONVERSION_OP(trunc, 16, 32, u16, u32)
	DEFINE_STATIC_CAST_CONVERSION_OP(trunc, 16, 64, u16, u64)
	DEFINE_STATIC_CAST_CONVERSION_OP(trunc, 32, 64, u32, u64)


#define DEFINE_INT_TO_FLOAT(DST_SIZE)                                                    \
	DEFINE_STATIC_CAST_CONVERSION_OP(sitofp, DST_SIZE, 8, FLOAT_##DST_SIZE##_TYPE, i8)   \
	DEFINE_STATIC_CAST_CONVERSION_OP(uitofp, DST_SIZE, 8, FLOAT_##DST_SIZE##_TYPE, u8)   \
	DEFINE_STATIC_CAST_CONVERSION_OP(sitofp, DST_SIZE, 16, FLOAT_##DST_SIZE##_TYPE, i16) \
	DEFINE_STATIC_CAST_CONVERSION_OP(uitofp, DST_SIZE, 16, FLOAT_##DST_SIZE##_TYPE, u16) \
	DEFINE_STATIC_CAST_CONVERSION_OP(sitofp, DST_SIZE, 32, FLOAT_##DST_SIZE##_TYPE, i32) \
	DEFINE_STATIC_CAST_CONVERSION_OP(uitofp, DST_SIZE, 32, FLOAT_##DST_SIZE##_TYPE, u32) \
	DEFINE_STATIC_CAST_CONVERSION_OP(sitofp, DST_SIZE, 64, FLOAT_##DST_SIZE##_TYPE, i64) \
	DEFINE_STATIC_CAST_CONVERSION_OP(uitofp, DST_SIZE, 64, FLOAT_##DST_SIZE##_TYPE, u64)

	DEFINE_INT_TO_FLOAT(32)
	DEFINE_INT_TO_FLOAT(64)


#define DEFINE_FPTOSI_OP(NAME, DST_SIZE, SRC_SIZE, DST_TYPE, SRC_TYPE)                        \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##DST_SIZE##_p##SRC_SIZE)(FUNCTION_ARGS) {        \
		{                                                                                     \
			using IntT   = DST_TYPE;                                                          \
			using FloatT = SRC_TYPE;                                                          \
			auto x       = READ_FROM_PLACE_ARG(FloatT, instr->arg1);                          \
			IntT res;                                                                         \
			if (std::isnan(x)) {                                                              \
				res = IntT{ 0 };                                                              \
			} else {                                                                          \
				constexpr FloatT min = static_cast<FloatT>(std::numeric_limits<IntT>::min()); \
				constexpr FloatT max = static_cast<FloatT>(std::numeric_limits<IntT>::max()); \
				if (x <= min) {                                                               \
					res = std::numeric_limits<IntT>::min();                                   \
				} else if (x >= max) {                                                        \
					res = std::numeric_limits<IntT>::max();                                   \
				} else {                                                                      \
					res = static_cast<IntT>(x);                                               \
				}                                                                             \
			}                                                                                 \
			WRITE_TO_PLACE_ARG(IntT, instr->arg0, res);                                       \
		}                                                                                     \
		FUNCTION_CONT(1);                                                                     \
	}

#define DEFINE_FPTOUI_OP(NAME, DST_SIZE, SRC_SIZE, DST_TYPE, SRC_TYPE)                         \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_p##DST_SIZE##_p##SRC_SIZE)(FUNCTION_ARGS) {         \
		{                                                                                      \
			using UIntT  = DST_TYPE;                                                           \
			using FloatT = SRC_TYPE;                                                           \
			auto  x      = READ_FROM_PLACE_ARG(FloatT, instr->arg1);                           \
			UIntT res;                                                                         \
			if (std::isnan(x)) {                                                               \
				res = UIntT{ 0 };                                                              \
			} else {                                                                           \
				constexpr FloatT max = static_cast<FloatT>(std::numeric_limits<UIntT>::max()); \
				if (x <= FloatT{ 0 }) {                                                        \
					res = UIntT{ 0 };                                                          \
				} else if (x >= max) {                                                         \
					res = std::numeric_limits<UIntT>::max();                                   \
				} else {                                                                       \
					res = static_cast<UIntT>(x);                                               \
				}                                                                              \
			}                                                                                  \
			WRITE_TO_PLACE_ARG(UIntT, instr->arg0, res);                                       \
		}                                                                                      \
		FUNCTION_CONT(1);                                                                      \
	}

#define DEFINE_FLOAT_TO_INT(SRC_SIZE)                                            \
	DEFINE_FPTOSI_OP(fptosi, 8, SRC_SIZE, std::int8_t, FLOAT_##SRC_SIZE##_TYPE)  \
	DEFINE_FPTOUI_OP(fptoui, 8, SRC_SIZE, std::uint8_t, FLOAT_##SRC_SIZE##_TYPE) \
	DEFINE_FPTOSI_OP(fptosi, 16, SRC_SIZE, i16, FLOAT_##SRC_SIZE##_TYPE)         \
	DEFINE_FPTOUI_OP(fptoui, 16, SRC_SIZE, u16, FLOAT_##SRC_SIZE##_TYPE)         \
	DEFINE_FPTOSI_OP(fptosi, 32, SRC_SIZE, i32, FLOAT_##SRC_SIZE##_TYPE)         \
	DEFINE_FPTOUI_OP(fptoui, 32, SRC_SIZE, u32, FLOAT_##SRC_SIZE##_TYPE)         \
	DEFINE_FPTOSI_OP(fptosi, 64, SRC_SIZE, i64, FLOAT_##SRC_SIZE##_TYPE)         \
	DEFINE_FPTOUI_OP(fptoui, 64, SRC_SIZE, u64, FLOAT_##SRC_SIZE##_TYPE)

	DEFINE_FLOAT_TO_INT(32)
	DEFINE_FLOAT_TO_INT(64)

	DEFINE_STATIC_CAST_CONVERSION_OP(fptrunc, 32, 64, FLOAT_32_TYPE, FLOAT_64_TYPE)
	DEFINE_STATIC_CAST_CONVERSION_OP(fpext, 64, 32, FLOAT_64_TYPE, FLOAT_32_TYPE)

	RETURN_TYPE OpFuns::OPCODE_NAME(breakpoint)(FUNCTION_ARGS) {
		{
			save_execution_state(instr, local_stack, frame, thread);

			thread.handleBreakpoint();
			thread.executeOneStep();

			// Restore current flow.
			// They can be changed when doing "step by step" execution.
			frame       = thread.runtime_data.frame_stack_current;
			instr       = frame->instr;
			local_stack = frame->local_stack;
		}

		FUNCTION_CONT(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(stepGil)(FUNCTION_ARGS) {
		{ thread.stepGil(); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(initFromVmValue)(FUNCTION_ARGS) {
		{
			const VmValue& vm_value = *std::bit_cast<const VmValue*>(instr->arg0);
			performInit(instr, local_stack, frame, thread, vm_value.type);
			vm_value.exportData({ Ref(frame->local_block_ref_stack_end[-1]), 0 });
		}
		FUNCTION_CONT(1);
	}
}

#undef OPCODE_NAME
#undef FUNCTION_ARGS
#undef FUNCTION_CONT
