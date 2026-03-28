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
#ifdef ENABLE_JIT
	#include <vm/core/jit/jit_compiler.hpp>
#endif
#include <base/types/floats.hpp>

#include <vm/core/process/builtin_functions.hpp>
#include <vm/core/process/exceptions.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/safe_vmprocess.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/core/thread/vmvalue.hpp>
#include <vm/utils/interpret.hpp>

#include <cmath>
#include <limits>


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
	#define OP_FUN                             vm::OpFun
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
	// macro. This means, every other instruction will jump to the next at the end of it with
	// `FUNCTION_CONT`/`FUNCTION_CONT_CHECK_STRATEGY`, so the the only way to end execution is to
	// use this opcode. It also requires different macro surrounding the function call in the
	// switch case because in this approach we can't end execution from
	// within the function, but we have to add some instructions on the outside of it. Hence we use
	// the `OP_CASE_END` macro that adds `goto End` instruction, residing after opcode function,
	// inside interpreter loop.
	RETURN_TYPE OpFuns::OPCODE_NAME(exit)(FUNCTION_ARGS) {
		{ CORE_ASSERT(frame->block_stack.size() == 1, "Invalid start function."); }
		IF_TC(return;)
	}

#define DEFINE_MOVE_OPS(BITS_SIZE, TYPE)                                                          \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {                      \
		{ writeToStack<TYPE>(local_stack, instr->arg0, safeReadObjectBytes<TYPE>(instr->arg1)); } \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_g##BITS_SIZE##_imm)(FUNCTION_ARGS) {                      \
		{ WRITE_TO_GLOBAL(TYPE, instr->arg0, safeReadObjectBytes<TYPE>(instr->arg1)); }           \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {             \
		{                                                                                         \
			safeWriteBytes<TYPE>(                                                                 \
				local_stack, readFromStack<TYPE>(local_stack, instr->arg1), instr->arg0           \
			);                                                                                    \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_g##BITS_SIZE##_g##BITS_SIZE)(FUNCTION_ARGS) {             \
		{                                                                                         \
			const auto value = READ_FROM_GLOBAL(TYPE, instr->arg1);                               \
			WRITE_TO_GLOBAL(TYPE, instr->arg0, value);                                            \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_g##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {             \
		{                                                                                         \
			const auto value = readFromStack<TYPE>(local_stack, instr->arg1);                     \
			WRITE_TO_GLOBAL(TYPE, instr->arg0, value);                                            \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l##BITS_SIZE##_g##BITS_SIZE)(FUNCTION_ARGS) {             \
		{                                                                                         \
			const auto value = READ_FROM_GLOBAL(TYPE, instr->arg1);                               \
			writeToStack<TYPE>(local_stack, instr->arg0, value);                                  \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(cmov_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {            \
		{                                                                                         \
			if (frame->flags.flag) {                                                              \
				const auto value = readFromStack<TYPE>(local_stack, instr->arg1);                 \
				writeToStack<TYPE>(local_stack, instr->arg0, value);                              \
			}                                                                                     \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}                                                                                             \
	RETURN_TYPE OpFuns::OPCODE_NAME(cmov_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {                     \
		{                                                                                         \
			if (frame->flags.flag)                                                                \
				writeToStack<TYPE>(                                                               \
					local_stack, instr->arg0, vm::safeReadObjectBytes<TYPE>(instr->arg1)          \
				);                                                                                \
		}                                                                                         \
		FUNCTION_CONT(1);                                                                         \
	}

	DEFINE_MOVE_OPS(64, u64)
	DEFINE_MOVE_OPS(32, u32)
	DEFINE_MOVE_OPS(16, u16)
	DEFINE_MOVE_OPS(8, u8)

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lptr_gptr)(FUNCTION_ARGS) {
		{
			const auto dst     = readFromStack<Pointer>(local_stack, instr->arg0);
			auto       src     = READ_FROM_GLOBAL(Pointer, instr->arg1);
			const auto new_dst = thread.process_memory.updatePointerAssignment(dst, src);
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_gptr_lptr)(FUNCTION_ARGS) {
		{
			const auto dst     = READ_FROM_GLOBAL(Pointer, instr->arg0);
			auto       src     = readFromStack<Pointer>(local_stack, instr->arg1);
			const auto new_dst = thread.process_memory.updatePointerAssignment(dst, src);
			WRITE_TO_GLOBAL(Pointer, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

#define DEFINE_ARITHMETIC_OP(NAME, BITS_SIZE, TYPE, OP)                                  \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) { \
		{                                                                                \
			auto       lhs = readFromStack<TYPE>(local_stack, instr->arg0);              \
			const auto rhs = readFromStack<TYPE>(local_stack, instr->arg1);              \
			lhs OP     rhs;                                                              \
			writeToStack<TYPE>(local_stack, instr->arg0, lhs);                           \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}                                                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {          \
		{                                                                                \
			auto       lhs = readFromStack<TYPE>(local_stack, instr->arg0);              \
			const auto rhs = safeReadObjectBytes<TYPE>(instr->arg1);                     \
			lhs        OP static_cast<TYPE>(rhs);                                        \
			writeToStack<TYPE>(local_stack, instr->arg0, lhs);                           \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}

#define DEFINE_DIVISION_LIKE_OP(NAME, BITS_SIZE, TYPE, OP)                                \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {  \
		{                                                                                 \
			auto       lhs = readFromStack<TYPE>(local_stack, instr->arg0);               \
			const auto rhs = readFromStack<TYPE>(local_stack, instr->arg1);               \
			if (rhs == static_cast<TYPE>(0)) throw exceptions::VMZeroDivisionException(); \
			lhs = static_cast<TYPE>(lhs OP rhs);                                          \
			writeToStack<TYPE>(local_stack, instr->arg0, lhs);                            \
		}                                                                                 \
		FUNCTION_CONT(1);                                                                 \
	}                                                                                     \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {           \
		{                                                                                 \
			auto lhs = readFromStack<TYPE>(local_stack, instr->arg0);                     \
			auto rhs = safeReadObjectBytes<TYPE>(instr->arg1);                            \
			if (rhs == static_cast<TYPE>(0)) throw exceptions::VMZeroDivisionException(); \
			lhs = static_cast<TYPE>(lhs OP rhs);                                          \
			writeToStack<TYPE>(local_stack, instr->arg0, lhs);                            \
		}                                                                                 \
		FUNCTION_CONT(1);                                                                 \
	}

#define DEFINE_NEGATION_OP(NAME, BITS_SIZE, TYPE)                                        \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE)(FUNCTION_ARGS) {                \
		{                                                                                \
			auto value = readFromStack<TYPE>(local_stack, instr->arg0);                  \
			writeToStack<TYPE>(local_stack, instr->arg0, value * static_cast<TYPE>(-1)); \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
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


#define DEFINE_BOOLEAN_OP(NAME, OP)                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l8##_l8)(FUNCTION_ARGS) {                   \
		{                                                                              \
			const bool lhs = (readFromStack<u8>(local_stack, instr->arg0) != u8{ 0 }); \
			const bool rhs = (readFromStack<u8>(local_stack, instr->arg1) != u8{ 0 }); \
			writeToStack<u8>(local_stack, instr->arg0, static_cast<u8>(lhs OP rhs));   \
		}                                                                              \
		FUNCTION_CONT(1);                                                              \
	}                                                                                  \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l8##_imm)(FUNCTION_ARGS) {                  \
		{                                                                              \
			const bool lhs = (readFromStack<u8>(local_stack, instr->arg0) != u8{ 0 }); \
			const bool rhs = (safeReadObjectBytes<u8>(instr->arg1) != u8{ 0 });        \
			writeToStack<u8>(local_stack, instr->arg0, static_cast<u8>(lhs OP rhs));   \
		}                                                                              \
		FUNCTION_CONT(1);                                                              \
	}

	DEFINE_BOOLEAN_OP(log_and, &&)
	DEFINE_BOOLEAN_OP(log_or, ||)
	DEFINE_BOOLEAN_OP(log_xor, !=)

	RETURN_TYPE OpFuns::OPCODE_NAME(log_not_l8)(FUNCTION_ARGS) {
		{
			bool result = (readFromStack<u8>(local_stack, instr->arg0) == u8{ 0 });
			writeToStack<u8>(local_stack, instr->arg0, (result ? u8{ 1 } : u8{ 0 }));
		}
		FUNCTION_CONT(1);
	}

#define DEFINE_COMPARISON_OP(NAME, BITS_SIZE, TYPE, OP)                                  \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) { \
		{                                                                                \
			frame->flags.flag = readFromStack<TYPE>(local_stack, instr->arg0)            \
				OP readFromStack<TYPE>(local_stack, instr->arg1);                        \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}                                                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {          \
		{                                                                                \
			frame->flags.flag = readFromStack<TYPE>(local_stack, instr->arg0)            \
				OP safeReadObjectBytes<TYPE>(instr->arg1);                               \
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

	RETURN_TYPE OpFuns::OPCODE_NAME(cmpNull_lptr)(FUNCTION_ARGS) {
		{
			auto pointer      = readFromStack<Pointer>(local_stack, instr->arg0);
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

	RETURN_TYPE OpFuns::OPCODE_NAME(call_func)(FUNCTION_ARGS) {
		{ performFunctionCall(instr, local_stack, frame, thread, instr->arg0); }
		// After acquiring the `executing_code` of the new function we have instruction pointer
		// (`instr`) pointing at the first instruction of the new function, so moving forward by one
		// would mean that we skipped the first instruction. That's why we move forward zero
		// instructions. For future returns, the first instruction that should be executed after
		// call is saved on frame so that `op_ret`s have to move forward zero instructions after
		// restoring `instr` from frame.
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

#ifdef ENABLE_JIT
	RETURN_TYPE OpFuns::OPCODE_NAME(jit_call_entrypoint)(FUNCTION_ARGS) {
		{
			auto& jit_data = thread.jit_data;
			auto  func_id  = instr->arg0;

			// @TODO: #2126 manage the size when inserting new code
			if (jit_data.size() <= func_id) jit_data.resize(2 * func_id + 2);

			jit::JitFuncData& my_data = jit_data[func_id];

			auto run_compiled = [&]() {
				performFunctionCall(instr, local_stack, frame, thread, func_id);
				(*my_data.func_ptr)(&instr, &local_stack, &frame, &thread);
			};

			if (my_data.func_ptr) {
				// is already compiled
				run_compiled();
			} else if (0 < my_data.until_compilation) {
				// should be compiled later
				--my_data.until_compilation;
				performFunctionCall(instr, local_stack, frame, thread, func_id);
			} else {
				// should be compiled now
				const low::LowFuncData& current_function
					= thread.executing_program->getFunctions()[func_id];

				MRef<jit::JitOpFun> compiled = jit::compileLLVM(current_function);

				CORE_ASSERT(compiled, "Compiled function pointer shouldn't be nullptr");
				my_data.func_ptr = compiled;

				run_compiled();
			}
		}
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}
#endif

	RETURN_TYPE OpFuns::OPCODE_NAME(call_builtinfunc)(FUNCTION_ARGS) {
		{
			auto builtin_id         = static_cast<builtins::BuiltinFunctionID>(instr->arg0);
			auto function_signature = builtins::getBuiltinFunctionSignature(builtin_id);
			auto arg_count          = function_signature->parameters.size();

			std::vector<Box<VmValue>> args;
			u64                       first_arg_idx = frame->block_stack.size() - arg_count;

			// Create VmValue objects from local arguments.
			for (u64 i = 0; i < arg_count; i++) {
				const base::StrID arg_type  = function_signature->parameters[i];
				TypeCRef          real_type = thread.executing_program->getTypes().at(arg_type);
				auto              block     = frame->block_stack[first_arg_idx + i];
				args.push_back(thread.process.createOwnedVmValue(real_type, Pointer(block, 0)));
			}

			base::Optional<Box<VmValue>> return_value = builtins::callBuiltinFunction(
				builtin_id,
				thread.executing_program->getTypes().at(function_signature->result_type),
				thread.process,
				thread,
				args
			);

			if (return_value.has_value()) {
				auto value = std::move(return_value.value());
				value->exportData(Pointer(frame->block_stack[first_arg_idx - 1], 0));
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
			auto ext_func_id = instr->arg0;
			auto ext_func    = thread.executing_program->getExternCFunctions().at(ext_func_id);

			auto arg_count = ext_func->parameters.size();
			bool is_void   = ext_func->result_type->getName() == "void";

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
				u64 result_value_idx = frame->block_stack.size() - arg_count - (is_void ? 0 : 1);


				auto ext_result_destination = frame->block_stack[result_value_idx];
				auto result_view = thread.process_memory.getBlockViewUnsafe(ext_result_destination);

				// Prepare arguments and call the function.
				byte* result_pointer = result_view.getBegin();
				byte* args_pointer
					= result_pointer + (is_void ? 0 : ext_func->result_type->getSize().asInt());

				ext_func->function_pointer(result_pointer, args_pointer);

				for (u64 i = 0; i < arg_count; i++) performDeinit(frame, thread);
			}
		}

		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(set_threadctx)(FUNCTION_ARGS) {
		{
			auto& called_func = thread.executing_program->getFunctions()[instr->arg0];
			thread.setThreadCtx(called_func.name.str());
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(virtual_call_lptr_method)(FUNCTION_ARGS) {
		{
			const auto pointer = readFromStack<Pointer>(local_stack, instr->arg0);

			// Objects are guaranteed to hold inheritance metadata pointers as their first field.
			// This is verified by static verification.

			const auto  view             = Memory::getPointerData(pointer, sizeof(Type*));
			const auto* inh_meta_pointer = readFromView<const Type*>(view);

			if (inh_meta_pointer == nullptr) throw exceptions::VMVtableUnset();

			const auto inh_metadata = inh_meta_pointer->getInheritanceMetadata().value();
			const auto method_name  = thread.executing_program->getMethodNamePool()[instr->arg1];
			const auto implementation_name = inh_metadata->vtable[method_name];

			const usize function_id
				= *thread.executing_program->getFunctions().idOf(implementation_name);

			performFunctionCall(instr, local_stack, frame, thread, function_id);
		}
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret_tailcall_func)(FUNCTION_ARGS) {
		{
			auto  function_id       = static_cast<usize>(instr->arg0);
			auto& function          = thread.executing_program->getFunctions()[function_id];
			instr                   = function.bc.data();
			frame->current_function = &function;

			if (local_stack + function.local_stack_size > thread.runtime_data.local_stack_end)
				throw exceptions::VMStackOverflowException();
		}
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret)(FUNCTION_ARGS) {
		{
			// Frame of the function we're returning from.
			auto*      callee_frame = frame;
			const bool void_func    = frame->current_function->result_type->getName() == "void";

			// We have to update values passed in arguments.
			// Old `instr` and `local_stack` are stored on the previous frame.
			// Previous frame is just before current frame in the array, so that
			// substracting one from the pointer will give us the previous frame.
			// The `instr`, `local_stack` and `frame` values should be restored from the previous
			// call stack frame.
			frame--;  // This is now the caller's frame.

			while (!callee_frame->block_stack.empty()) {
				auto block = callee_frame->block_stack.back();

				// We're returning from a non-void function, so the last block on the stack is the
				// return value. It's being used by the caller so we don't free it.
				if (void_func || callee_frame->block_stack.size() != 1) {
					thread.process_memory.freeBlockData(block);
					thread.process_memory.decreaseBlockRefcount(block);
				}

				callee_frame->block_stack.pop_back();
			}
			callee_frame->resetFrameData();

			// Load previous frame.
			instr       = frame->instr;  // This is already a pointer to next instr.
			local_stack = frame->local_stack;
		}
		// Here the argument is `0` because of the convention defined in the op_call_func.
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(init_lany_type)(FUNCTION_ARGS) {
		{ performInit(instr, local_stack, frame, thread, TypeID(instr->arg1)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(deinit)(FUNCTION_ARGS) {
		{ performDeinit(frame, thread); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(input_l64)(FUNCTION_ARGS) {
		{
			thread.setProcessStatus(api::WaitingForInput{});
			i64 io_value = thread.process.getIO().getInput<i64>(thread);
			writeToStack<i64>(local_stack, instr->arg0, io_value);
			thread.setProcessStatus(api::Running{});
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(output_l64)(FUNCTION_ARGS) {
		{ thread.process.getIO().writeOutput(readFromStack<u64>(local_stack, instr->arg0)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(input_l32)(FUNCTION_ARGS) {
		{
			thread.setProcessStatus(api::WaitingForInput{});
			i32 io_value = thread.process.getIO().getInput<i32>(thread);
			writeToStack<i32>(local_stack, instr->arg0, io_value);
			thread.setProcessStatus(api::Running{});
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(output_l32)(FUNCTION_ARGS) {
		{ thread.process.getIO().writeOutput(readFromStack<u32>(local_stack, instr->arg0)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(strOutput_lptr)(FUNCTION_ARGS) {
		{
			auto ptr = readFromStack<Pointer>(local_stack, instr->arg0);

			auto block      = ptr.getBlock();
			auto block_id   = thread.process_memory.requestBlockID(block);
			auto block_data = thread.process_memory.requestBlockData(block_id);
			auto str_data   = block_data.stdString();
			thread.process.getIO().writeOutput(str_data.substr(0, str_data.size() - 1));
		}
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

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_type_type)(FUNCTION_ARGS) {
		CORE_PANIC("ext_type_type not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(alloc_lptr_type)(FUNCTION_ARGS) {
		{
			const auto dst = readFromStack<Pointer>(local_stack, instr->arg0);
			auto       type
				= thread.executing_program->getTypes().at(vm::TypeID(static_cast<u32>(instr->arg1)));
			auto       block   = thread.process_memory.allocateHeap(type);
			const auto new_dst = thread.process_memory.updatePointerAssignment(dst, { block, 0 });
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(free_lptr)(FUNCTION_ARGS) {
		{
			if (auto ptr = readFromStack<Pointer>(local_stack, instr->arg0))
				thread.process_memory.freeBlockData(ptr.getBlock());
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ref_lptr_lany)(FUNCTION_ARGS) {
		{
			const auto dst       = readFromStack<Pointer>(local_stack, instr->arg0);
			auto       block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg1)];
			auto       block     = frame->block_stack[block_idx];
			const auto new_dst   = thread.process_memory.updatePointerAssignment(dst, { block, 0 });
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ref_lptr_gany)(FUNCTION_ARGS) {
		{
			const auto dst       = readFromStack<Pointer>(local_stack, instr->arg0);
			auto       src_block = GET_GLOBAL_BLOCK(instr->arg1);
			const auto new_dst
				= thread.process_memory.updatePointerAssignment(dst, { src_block, 0 });
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lptr_lptr)(FUNCTION_ARGS) {
		{
			const auto    dst     = readFromStack<Pointer>(local_stack, instr->arg0);
			const auto    src     = readFromStack<Pointer>(local_stack, instr->arg1);
			const Pointer new_dst = thread.process_memory.updatePointerAssignment(dst, src);
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lopq_lopq)(FUNCTION_ARGS) {
		{
			auto       dst_block_idx = frame->local_offset_to_block_idx[instr->arg0];
			auto       dst_block     = frame->block_stack[dst_block_idx];
			const auto type_size = thread.process_memory.getBlockType(dst_block)->getSize().asInt();
			std::memcpy(local_stack + instr->arg0, local_stack + instr->arg1, type_size);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_gopq_lopq)(FUNCTION_ARGS) {
		{
			auto dst_block = GET_GLOBAL_BLOCK(instr->arg0);
			std::memcpy(
				thread.process_memory.getBlockViewUnsafe(dst_block).getBegin(),
				local_stack + instr->arg1,
				thread.process_memory.getBlockType(dst_block)->getSize().asInt()
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lopq_gopq)(FUNCTION_ARGS) {
		{
			auto src_block = GET_GLOBAL_BLOCK(instr->arg1);
			std::memcpy(
				local_stack + instr->arg0,
				thread.process_memory.getBlockViewUnsafe(src_block).getBegin(),
				thread.process_memory.getBlockType(src_block)->getSize().asInt()
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lopq_imm)(FUNCTION_ARGS) {
		{
			// @TODO: #1728 remove this evil instruction
			const void* value = safeReadObjectBytes<void*>(instr->arg1);
			writeToStack(local_stack, instr->arg0, value);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lste_lste)(FUNCTION_ARGS) {
		{
			auto dst_block_idx = frame->local_offset_to_block_idx[instr->arg0];
			auto dst_block     = frame->block_stack[dst_block_idx];
			auto src_block_idx = frame->local_offset_to_block_idx[instr->arg1];
			auto src_block     = frame->block_stack[src_block_idx];
			thread.process_memory.copyPointedData(
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block)
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_gste_gste)(FUNCTION_ARGS) {
		{
			auto dst_block = GET_GLOBAL_BLOCK(instr->arg0);
			auto src_block = GET_GLOBAL_BLOCK(instr->arg1);
			thread.process_memory.copyPointedData(
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block)
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lste_gste)(FUNCTION_ARGS) {
		{
			auto dst_block = frame->block_stack[frame->local_offset_to_block_idx[instr->arg0]];
			auto src_block = GET_GLOBAL_BLOCK(instr->arg1);
			thread.process_memory.copyPointedData(
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block)
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_gste_lste)(FUNCTION_ARGS) {
		{
			auto dst_block = GET_GLOBAL_BLOCK(instr->arg0);
			auto src_block = frame->block_stack[frame->local_offset_to_block_idx[instr->arg1]];
			thread.process_memory.copyPointedData(
				{ dst_block, 0 }, { src_block, 0 }, thread.process_memory.getBlockType(dst_block)
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(setNull_lptr)(FUNCTION_ARGS) {
		{
			const auto    dst = readFromStack<Pointer>(local_stack, instr->arg0);
			const Pointer new_dst
				= thread.process_memory.updatePointerAssignment(dst, Pointer::null());
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(setVTable_lptr_type)(FUNCTION_ARGS) {
		{
			auto pointer = readFromStack<Pointer>(local_stack, instr->arg0);
			auto type    = thread.executing_program->getTypes().at(
                TypeID(base::safeIntConv<usize>(instr->arg1))
            );

			// Objects hold vtable pointer as their first field.
			auto view = thread.process_memory.getPointerData(pointer, sizeof(Type*));
			// This writes a pointer to the type at object's first field.
			writeToView<const Type*>(view, type.get());
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(resetVTable_lptr)(FUNCTION_ARGS) {
		{
			auto pointer = readFromStack<Pointer>(local_stack, instr->arg0);
			auto view    = thread.process_memory.getPointerData(pointer, sizeof(Type*));
			writeToView<const Type*>(view, nullptr);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantSetInner_lvnt_type)(FUNCTION_ARGS) {
		{
			auto variant_block_index = frame->local_offset_to_block_idx[instr->arg0];
			auto variant_block       = frame->block_stack[variant_block_index];
			auto alt_type_id         = TypeID(instr->arg1);
			auto variant_type_id     = TypeID(instr[1].arg0);
			OpFuns::setVariantType(thread, Pointer(variant_block, 0), alt_type_id, variant_type_id);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantGetInner_lptr_lvnt)(FUNCTION_ARGS) {
		{
			const auto dst                 = readFromStack<Pointer>(local_stack, instr->arg0);
			auto       variant_block_index = frame->local_offset_to_block_idx[u64(instr->arg1)];
			auto       variant_block       = frame->block_stack[variant_block_index];
			auto       alt_type_id         = TypeID(instr[1].arg0);
			auto       variant_type_id     = TypeID(instr[1].arg1);

			const auto new_dst = thread.process_memory.updatePointerAssignment(
				dst,
				OpFuns::getVariantPtr(
					thread, Pointer(variant_block, 0), alt_type_id, variant_type_id
				)
			);
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantSetInner_lptr_type)(FUNCTION_ARGS) {
		{
			auto variant_pointer = readFromStack<Pointer>(local_stack, instr->arg0);
			auto alt_type_id     = TypeID(instr->arg1);
			auto variant_type_id = TypeID(instr[1].arg0);
			OpFuns::setVariantType(thread, variant_pointer, alt_type_id, variant_type_id);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(variantGetInner_lptr_lptr)(FUNCTION_ARGS) {
		{
			const auto dst             = readFromStack<Pointer>(local_stack, instr->arg0);
			auto       variant_pointer = readFromStack<Pointer>(local_stack, instr->arg1);
			auto       alt_type_id     = TypeID(instr[1].arg0);
			auto       variant_type_id = TypeID(instr[1].arg1);

			const auto new_dst = thread.process_memory.updatePointerAssignment(
				dst, OpFuns::getVariantPtr(thread, variant_pointer, alt_type_id, variant_type_id)
			);
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}

		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(upcast_lptr_lptr)(FUNCTION_ARGS) {
		{
			// Same as move_lptr_lptr, treated differently by static analysis.
			const auto    dst     = readFromStack<Pointer>(local_stack, instr->arg0);
			const auto    src     = readFromStack<Pointer>(local_stack, instr->arg1);
			const Pointer new_dst = thread.process_memory.updatePointerAssignment(dst, src);
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(downcast_lptr_lptr)(FUNCTION_ARGS) {
		{
			const auto dst = readFromStack<Pointer>(local_stack, instr->arg0);
			const auto src = readFromStack<Pointer>(local_stack, instr->arg1);

			auto dst_type = thread.executing_program->getTypes().at(TypeID(instr[1].arg0));

			// Classes are guaranteed to hold vtable pointer as their first field.
			auto        view         = thread.process_memory.getPointerData(src, sizeof(Type*));
			const auto* src_ptr      = readFromView<const Type*>(view);
			auto        cast_allowed = src_ptr->inheritsFrom(dst_type);

			const Pointer new_dst = thread.process_memory.updatePointerAssignment(
				dst, cast_allowed ? src : Pointer::null()
			);
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(store_lptr_lany)(FUNCTION_ARGS) {
		{
			auto dst_pointer = readFromStack<Pointer>(local_stack, instr->arg0);

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

			auto src_pointer = readFromStack<Pointer>(local_stack, instr->arg1);

			auto type = Memory::getBlockType(dst_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLea_lptr_lptr)(FUNCTION_ARGS) {
		{
			const auto dst    = readFromStack<Pointer>(local_stack, instr->arg0);
			auto       src    = readFromStack<Pointer>(local_stack, instr->arg1);
			auto       offset = static_cast<usize>(instr[1].arg0);

			const Pointer new_dst
				= thread.process_memory.updatePointerAssignment(dst, { src.getBlock(), offset });
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structStore_lptr_lany)(FUNCTION_ARGS) {
		{
			auto dst_pointer  = readFromStack<Pointer>(local_stack, instr->arg0);
			auto field_offset = safeReadObjectBytes<i64>(instr[1].arg0);
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

			auto src_pointer  = readFromStack<Pointer>(local_stack, instr->arg1);
			auto field_offset = safeReadObjectBytes<i64>(instr[1].arg0);
			src_pointer.movePointer(field_offset);

			auto type = Memory::getBlockType(dst_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLea_lptr_lste)(FUNCTION_ARGS) {
		{
			const auto dst       = readFromStack<Pointer>(local_stack, instr->arg0);
			const auto src       = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg1)];
			const auto src_block = frame->block_stack[src];
			auto       offset    = static_cast<usize>(instr[1].arg0);

			const Pointer new_dst
				= thread.process_memory.updatePointerAssignment(dst, { src_block, offset });
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structStore_lste_lany)(FUNCTION_ARGS) {
		{
			auto dst_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg0)];
			auto dst_block     = frame->block_stack[dst_block_idx];
			auto dst_pointer   = Pointer(dst_block, 0);

			auto field_offset = safeReadObjectBytes<i64>(instr[1].arg0);
			dst_pointer.movePointer(field_offset);

			auto src_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg1)];
			auto src_block     = frame->block_stack[src_block_idx];
			auto src_pointer   = Pointer(src_block, 0);

			auto type = Memory::getBlockType(src_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(structLoad_lany_lste)(FUNCTION_ARGS) {
		{
			auto dst_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg0)];
			auto dst_block     = frame->block_stack[dst_block_idx];
			auto dst_pointer   = Pointer(dst_block, 0);

			auto src_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg1)];
			auto src_block     = frame->block_stack[src_block_idx];
			auto src_pointer   = Pointer(src_block, 0);

			auto field_offset = safeReadObjectBytes<i64>(instr[1].arg0);
			src_pointer.movePointer(field_offset);

			auto type = Memory::getBlockType(dst_block);

			thread.process_memory.copyPointedData(dst_pointer, src_pointer, type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(dynTableLea_lptr_lptr)(FUNCTION_ARGS) {
		{
			auto dst          = readFromStack<Pointer>(local_stack, instr->arg0);
			auto tbl_pointer  = readFromStack<Pointer>(local_stack, instr->arg1);
			auto element_type = *Memory::getBlockType(tbl_pointer.getBlock())->getInnerType();
			auto index        = readFromStack<u64>(local_stack, instr[1].arg0);
			auto data_offset  = usize(element_type->getSize() * index);

			const Pointer new_dst = thread.process_memory.updatePointerAssignment(
				dst, { tbl_pointer.getBlock(), data_offset }
			);
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableLea_lptr_lptr)(FUNCTION_ARGS) {
		{
			auto dst          = readFromStack<Pointer>(local_stack, instr->arg0);
			auto tbl_pointer  = readFromStack<Pointer>(local_stack, instr->arg1);
			auto element_type = *Memory::getBlockType(tbl_pointer.getBlock())->getInnerType();
			auto index        = readFromStack<u64>(local_stack, instr[1].arg0);
			auto data_offset  = usize(element_type->getSize() * index);

			const Pointer new_dst = thread.process_memory.updatePointerAssignment(
				dst, { tbl_pointer.getBlock(), data_offset }
			);
			writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(dynTableStore_lptr_lany)(FUNCTION_ARGS) {
		{
			auto tbl_pointer  = readFromStack<Pointer>(local_stack, instr->arg0);
			auto index        = readFromStack<i64>(local_stack, instr[1].arg0);
			auto element_type = *Memory::getBlockType(tbl_pointer.getBlock())->getInnerType();
			auto data_offset  = index * static_cast<i64>(element_type->getSize());
			tbl_pointer.movePointer(data_offset);

			auto src_block_idx = frame->local_offset_to_block_idx[instr->arg1];
			auto src_block     = frame->block_stack[src_block_idx];
			auto src_pointer   = Pointer{ src_block, 0 };

			thread.process_memory.copyPointedData(tbl_pointer, src_pointer, element_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableStore_lptr_lany)(FUNCTION_ARGS) {
		{
			auto tbl_pointer  = readFromStack<Pointer>(local_stack, instr->arg0);
			auto element_type = *Memory::getBlockType(tbl_pointer.getBlock())->getInnerType();
			auto index        = readFromStack<i64>(local_stack, instr[1].arg0);
			auto data_offset  = index * static_cast<i64>(element_type->getSize());
			tbl_pointer.movePointer(data_offset);

			auto src_block_idx = frame->local_offset_to_block_idx[instr->arg1];
			auto src_block     = frame->block_stack[src_block_idx];
			auto src_pointer   = Pointer(src_block, 0);

			thread.process_memory.copyPointedData(tbl_pointer, src_pointer, element_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(dynTableLoad_lany_lptr)(FUNCTION_ARGS) {
		{
			auto dst_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg0)];
			auto dst_block     = frame->block_stack[dst_block_idx];
			auto dst_pointer   = Pointer(dst_block, 0);

			auto tbl_pointer = readFromStack<Pointer>(local_stack, instr->arg1);

			auto index        = readFromStack<i64>(local_stack, instr[1].arg0);
			auto element_type = *Memory::getBlockType(tbl_pointer.getBlock())->getInnerType();
			tbl_pointer.movePointer(index * static_cast<i64>(element_type->getSize()));

			thread.process_memory.copyPointedData(dst_pointer, tbl_pointer, element_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(fixedSizeTableLoad_lany_lptr)(FUNCTION_ARGS) {
		{
			auto dst_block_idx = frame->local_offset_to_block_idx[static_cast<u64>(instr->arg0)];
			auto dst_block     = frame->block_stack[dst_block_idx];
			auto dst_pointer   = Pointer(dst_block, 0);

			auto tbl_pointer = readFromStack<Pointer>(local_stack, instr->arg1);

			auto index        = readFromStack<i64>(local_stack, instr[1].arg0);
			auto element_type = *Memory::getBlockType(tbl_pointer.getBlock())->getInnerType();
			tbl_pointer.movePointer(index * static_cast<i64>(element_type->getSize()));

			thread.process_memory.copyPointedData(dst_pointer, tbl_pointer, element_type);
		}
		FUNCTION_CONT(2);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(dynTableReAlloc_lptr_type)(FUNCTION_ARGS) {
		{
			auto tbl_pointer    = readFromStack<Pointer>(local_stack, instr->arg0);
			auto pointed_type   = thread.executing_program->getTypes().at(TypeID(instr->arg1));
			auto new_elem_count = readFromStack<u64>(local_stack, instr[1].arg0);

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
					writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
				}
			} else if (tbl_pointer.isNull()) {
				auto new_block
					= thread.process_memory.dynTableAllocateHeapN(pointed_type, new_elem_count);
				const Pointer new_dst
					= thread.process_memory.updatePointerAssignment(tbl_pointer, { new_block, 0 });
				writeToStack<Pointer>(local_stack, instr->arg0, new_dst);
			} else {
				auto tbl_block = tbl_pointer.getBlock();
				thread.process_memory.dynTableReallocateBlockDataN(tbl_block, new_elem_count);
			}
		}
		FUNCTION_CONT(2);
	}

#define DEFINE_STATIC_CAST_CONVERSION_OP(NAME, DST_SIZE, SRC_SIZE, DST_TYPE, SRC_TYPE)    \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##DST_SIZE##_l##SRC_SIZE)(FUNCTION_ARGS) {    \
		{                                                                                 \
			auto val = readFromStack<SRC_TYPE>(local_stack, instr->arg1);                 \
			writeToStack<DST_TYPE>(local_stack, instr->arg0, static_cast<DST_TYPE>(val)); \
		}                                                                                 \
		FUNCTION_CONT(1);                                                                 \
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
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##DST_SIZE##_l##SRC_SIZE)(FUNCTION_ARGS) {        \
		{                                                                                     \
			using IntT   = DST_TYPE;                                                          \
			using FloatT = SRC_TYPE;                                                          \
			auto x       = readFromStack<FloatT>(local_stack, instr->arg1);                   \
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
			writeToStack<IntT>(local_stack, instr->arg0, res);                                \
		}                                                                                     \
		FUNCTION_CONT(1);                                                                     \
	}

#define DEFINE_FPTOUI_OP(NAME, DST_SIZE, SRC_SIZE, DST_TYPE, SRC_TYPE)                         \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##DST_SIZE##_l##SRC_SIZE)(FUNCTION_ARGS) {         \
		{                                                                                      \
			using UIntT  = DST_TYPE;                                                           \
			using FloatT = SRC_TYPE;                                                           \
			auto  x      = readFromStack<FloatT>(local_stack, instr->arg1);                    \
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
			writeToStack<UIntT>(local_stack, instr->arg0, res);                                \
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

	RETURN_TYPE OpFuns::OPCODE_NAME(stepGil)(FUNCTION_ARGS) {
		{ thread.stepGil(); }
		FUNCTION_CONT(1);
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
