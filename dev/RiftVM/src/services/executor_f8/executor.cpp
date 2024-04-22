#include <base/ints.hpp>
#include <chrono>

#include <code_data/instruction.hpp>
#include <code_data/code.hpp>
#include <code_data/frame.hpp>
#include <services_data/type_metadata/type.hpp>
#include <supervisor/vcpu.hpp>
#include "op_case.hpp"
#include "executor.hpp"

#include <iostream>

#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <utility>

namespace vm {

	void Executor::handleExecutionStrategy() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Paused) setStatus(api::Paused{});
		pause_cv.wait(lock, [this] { return execution_strategy != ExecutionStrategy::Paused; });
		setStatus(api::Running{});
		if (execution_strategy == ExecutionStrategy::Stoped) throw KillCoreException{};
		if (execution_strategy == ExecutionStrategy::StepByStep)
			execution_strategy = ExecutionStrategy::Paused;
	}

	[[gnu::always_inline]]
	inline void Executor::handleExecutionStrategyIfNeeded() {
		if (isRunning) return;
		handleExecutionStrategy();
	}

	[[gnu::always_inline]]
	inline Frame Executor::internalInitFrame(
		base::borrow_ptr<Frame> previous_frame,
		VLADataReference        vla_ref,
		BlockId*                block_id_stack,
		StandardFunctionArgs&   args
	) {
		return Frame{
			.previous            = previous_frame,
			.continue_execution  = true,
			.instruction_pointer = 0,

			.regs      = Registers{ .p64_reg_0 = 0, .pointer_reg_0 = memory.nullPtr() },
			.flags     = FlagData{ .flag = false },
			.ret_val   = 0,
			.next_args = { 0, memory.nullPtr() },

			.local_stack_head    = 0,
			.block_id_stack      = block_id_stack,
			.block_id_stack_head = 0,

			.args = args,

			.vla_data_reference = vla_ref,
			.executor           = *this,
		};
	}

	// arguments can be passed in temp values in caller
	// return values must somehow be passed to caller
	// Idea:
	// Stack works like this:
	// [locals][temp]
	// locals are stack-like independently
	// temp are stack-like independently
	// when function call happens, following stuff happen:
	// r := callee ret size
	// a := callee a size
	// R := temp reserve r-bytes
	// A := temp reserve a-bytes
	// set arguments
	// callee receives pointer temp [R|A]
	// callee does its stuff, and puts stuff into R (memcpy to local if needed)
	// callee returns
	// temp pop A
	// temp use R
	//
	// Or stack like this:
	// [rets][args][locals][temp] + memcpy between callee and caller

	[[gnu::always_inline]]
	inline base::ModRawView Executor::internalDerefPointer(Pointer pointer) {
		auto block_id = pointer.getBlock();
		auto offset   = pointer.getOffset();

		auto block = memory.getBlock(block_id);
		auto type  = block->innerType();
		auto view  = block->deref(type, offset);

		RIFT_ASSERT(view.size() == type->getSize(), "Bad deref size");
		return view;
	}

	template<typename T>
	__attribute__((always_inline)) inline static T& derefStack(std::byte* stack, i64 position) {
		return *(reinterpret_cast<T*>(&stack[position]));
	}

	// NOTE: functions that implement opcodes (opfunctions) must be done this way:
	//
	// RETURN_TYPE OpFuns::op_<opcode_name>(OPFUN_ARGS) {
	//  {
	//    <function_body>
	//  }
	//  OPFUN_CONT(<step>, r1, r2, r3);
	// }
	//
	// Function body must be seperated from the scope of OPFUN_CONT to make sure
	// that all its destructors have been called before invoking next tail call.
	// Otherwise, the compiler may get confused and may schedule destructors from
	// the body after the next tail call, which then becomes a regular function
	// call and may cause the stack to explode.

	RETURN_TYPE OpFuns::op_handle_strategy(OPFUN_ARGS) {
		{ frame.executor.handleExecutionStrategy(); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mov_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = instr->arg1; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mov_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_cmov_l64_l64(OPFUN_ARGS) {
		{
			if (frame.flags.flag)
				derefStack<u64>(local_stack, instr->arg0)
					= derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mov_l64_r0(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = frame.regs.p64_reg_0; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mov_r0_l64(OPFUN_ARGS) {
		{ frame.regs.p64_reg_0 = derefStack<u64>(local_stack, instr->arg0); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mov_l64_pFuncArg(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = frame.args.p64_arg; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mov_lptr_ptrFuncArg(OPFUN_ARGS) {
		{ derefStack<Pointer>(local_stack, instr->arg0) = frame.args.pointer_arg; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_add_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) += derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_add_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) += instr->arg1; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_sub_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) -= derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_sub_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) -= instr->arg1; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mul_l64_imm(OPFUN_ARGS) {
		{
			// @TODO: check types
			derefStack<u64>(local_stack, instr->arg0) *= instr->arg1;
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mod_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) %= instr->arg1; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_mod_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) %= derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_div_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) /= instr->arg1; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_cmpEq_l64_l64(OPFUN_ARGS) {
		{
			frame.flags.flag = derefStack<u64>(local_stack, instr->arg0)
			                == derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_cmpEq_l64_imm(OPFUN_ARGS) {
		{ frame.flags.flag = derefStack<u64>(local_stack, instr->arg0) == instr->arg1; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_cmpG_l64_l64(OPFUN_ARGS) {
		{
			frame.flags.flag = derefStack<u64>(local_stack, instr->arg0)
			                 > derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_cmpG_l64_imm(OPFUN_ARGS) {
		{ frame.flags.flag = derefStack<u64>(local_stack, instr->arg0) > instr->arg1; }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_jmpRel_label(OPFUN_ARGS) {
		{
			IF_NOT_TC(frame.instruction_pointer += instr->arg0;)
			IF_TC(instr += instr->arg0;)
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_jmpRelIf_label(OPFUN_ARGS) {
		{
			if (frame.flags.flag) {
				IF_NOT_TC(frame.instruction_pointer += instr->arg0;)
				IF_TC(instr += instr->arg0;)
			}
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_jmpRelNotIf_label(OPFUN_ARGS) {
		{
			if (!frame.flags.flag) {
				IF_NOT_TC(frame.instruction_pointer += instr->arg0;)
				IF_TC(instr += instr->arg0;)
			}
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_setPtrArg_lptr(OPFUN_ARGS) {
		{ frame.next_args.pointer_arg = derefStack<Pointer>(local_stack, instr->arg0); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_setPArg_l64(OPFUN_ARGS) {
		{ frame.next_args.p64_arg = derefStack<i64>(local_stack, instr->arg0); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_call_func(OPFUN_ARGS) {
		{
			auto function_id     = instr->arg0;
			frame.regs.p64_reg_0 = frame.executor.internalCallFunction(
				// @TODO: this is not correct with flat frame
				base::borrow_ptr(&frame),
				frame.executor.executing_code->functions[function_id],
				frame.next_args
			);
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_ret_l64(OPFUN_ARGS) {
		{ frame.ret_val = derefStack<u64>(local_stack, instr->arg0); }
		IF_TC(return frame.ret_val;)
	}

	RETURN_TYPE OpFuns::op_ret_imm(OPFUN_ARGS) {
		{ frame.ret_val = instr->arg0; }
		IF_TC(return frame.ret_val;)
	}

	RETURN_TYPE OpFuns::op_init_type(OPFUN_ARGS) {
		{
			auto             type      = frame.executor.types.getType(vm::TypeId(instr->arg0));
			auto             type_size = type->getSize();
			base::ModRawView data(&local_stack[frame.local_stack_head], type_size);
			frame.local_stack_head += type_size;
			auto block = frame.executor.stack_allocator.makeTypeBlock(type, data);
			frame.block_id_stack[frame.block_id_stack_head++] = block;
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_deinit(OPFUN_ARGS) {
		{
			auto block = frame.block_id_stack[--frame.block_id_stack_head];
			frame.executor.stack_allocator.deleteBlock(block);
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_input_l64(OPFUN_ARGS) {
		{
			frame.executor.setStatus(api::WaitingForInput{});
			derefStack<i64>(local_stack, instr->arg0) = frame.executor.vcpu.getInput<i64>();
			frame.executor.setStatus(api::Running{});
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_output_l64(OPFUN_ARGS) {
		{ frame.executor.vcpu.writeOutput(derefStack<u64>(local_stack, instr->arg0)); }
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_nop(OPFUN_ARGS) { OPFUN_CONT(1, r1, r2, r3); }

	RETURN_TYPE OpFuns::op_ext_l64(OPFUN_ARGS) {
		RIFT_PANIC("ext_l64 not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::op_alloc_lptr_type(OPFUN_ARGS) {
		{
			auto type = frame.executor.types.getType(vm::TypeId(instr->arg1));
			// This is disasbled, because we don't want to pay performance for initializing it
			// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
			BlockId block;
			// NOLINTEND(cppcoreguidelines-pro-type-member-init)
			if (type->getKind() == vm::Type::Kind::StaticTable) {
				// @TODO: As noted in type.hpp, interface used below may change
				auto inner_type = type->getInnerType().value();
				auto table_size = type->getStaticTableSize().value();

				// @TODO: It is possible to access memory of the array via
				// mov_l64_imm, which should be at least detected, if not illegal
				block = frame.executor.dynamic_allocator.makeArrayBlock(inner_type, table_size);
			} else {
				block = frame.executor.dynamic_allocator.makeTypeBlock(type);
			}
			derefStack<Pointer>(local_stack, instr->arg0) = Pointer(block, 0);
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_free_lptr(OPFUN_ARGS) {
		{
			frame.executor.dynamic_allocator.deleteBlock(
				derefStack<Pointer>(local_stack, instr->arg0).getBlock()
			);
		}
		OPFUN_CONT(1, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_load_l64_lptr_ofs(OPFUN_ARGS) {
		constexpr const uint8_t view_size = 8;
		int                     next      = 1;
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg1);
			auto view    = frame.executor.internalDerefPointer(pointer);
			u64  idx     = 0;
#ifdef USE_TAIL_CALLS
			if (instr[1].opfun == OpFuns::op_ext_l64) {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#else
			if (static_cast<OpcodeFix8>(instr[1].opcode) == OpcodeFix8::ext_l64) {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				frame.instruction_pointer++;
			}
#endif

			// @TODO: this assert degrades performance by 5-10%
			// RIFT_ASSERT(view.size() == view_size, "bad type");

			std::memcpy(&local_stack[instr->arg0], view.getBegin() + idx * view_size, view_size);
		}
		OPFUN_CONT(next, r1, r2, r3);
	}

	RETURN_TYPE OpFuns::op_store_lptr_l64_ofs(OPFUN_ARGS) {
		constexpr const uint8_t view_size = 8;
		int                     next      = 1;
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg0);
			auto view    = frame.executor.internalDerefPointer(pointer);
			u64  idx     = 0;
#ifdef USE_TAIL_CALLS
			if (instr[1].opfun == OpFuns::op_ext_l64) {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#else
			if (static_cast<OpcodeFix8>(instr[1].opcode) == OpcodeFix8::ext_l64) {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				frame.instruction_pointer++;
			}
#endif

			// @TODO: this assert degrades performance by 5-10%
			// RIFT_ASSERT(view.size() == view_size, "bad type");

			std::memcpy(view.getBegin() + idx * view_size, &local_stack[instr->arg1], view_size);
		}
		OPFUN_CONT(next, r1, r2, r3);
	}

#ifndef USE_TAIL_CALLS
	__attribute__((always_inline)) void inline static peekNextInstruction(
		const std::span<const vm::Fix8Instruction>& bc,
		usize&                                      instruction_pointer,
		OpcodeFix8&                                 opcode
	) {
		Fix8Instruction instr = bc[instruction_pointer];

		opcode = static_cast<OpcodeFix8>(instr.opcode);
	}
#endif

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif

	u64 Executor::internalCallFunction(
		base::borrow_ptr<Frame> previous_frame, const FuncData& function, StandardFunctionArgs args
	) {
		// NOLINTBEGIN(modernize-avoid-c-arrays)
		// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
		// @TODO: sanity check should be added to see if function.stack_size is sensible small
		// @TODO: some generic code should be added to work with that does not support VLA
		std::byte local_stack[function.stack_size];


		// @TODO: static code analysis could be done to determine the smallest
		// possible stack size for block_ids of variables
		BlockId block_id_stack[function.stack_size];

		// NOLINTEND(cppcoreguidelines-avoid-c-arrays)
		// NOLINTEND(modernize-avoid-c-arrays)
		// @TODO: frame should hold pointers to data only, when
		// USE_FLAT_FRAME is on
		// This will be best made with code generation

#ifdef USE_FLAT_FRAME
		// Keeping these lines in, even if USE_FLAT_FRAME is off
		// for some very strange reason speeds up program by 10-20 %
		// It probably pokes compiler optimization in just the right way

		// @TODO: this should be autogenerated

		// Flat frame should technically allow for better compiler optimizations
		// but in practice are slower for some reason
		u64                  p64_reg_0           = 0;
		Pointer              pointer_reg_0       = memory.nullPtr();
		bool                 flag                = false;
		const auto           bc                  = function.bc;
		usize                instruction_pointer = 0;
		u64                  ret_val             = 0;
		StandardFunctionArgs next_args           = { 0, memory.nullPtr() };
#else
		Frame frame = internalInitFrame(
			previous_frame,
			{ static_cast<std::byte*>(local_stack) },
			static_cast<BlockId*>(block_id_stack),
			args
		);
#endif

#ifndef USE_TAIL_CALLS

	// Computed gotos labales:
	#ifdef USE_COMPUTED_GOTO
		constexpr static void* opcode_label[] = { LABEL_PTR(mov_l64_imm),

			                                      LABEL_PTR(mov_l64_l64),
			                                      LABEL_PTR(cmov_l64_l64),

			                                      LABEL_PTR(mov_l64_r0),
			                                      LABEL_PTR(mov_r0_l64),

			                                      LABEL_PTR(mov_l64_pFuncArg),
			                                      LABEL_PTR(mov_lptr_ptrFuncArg),

			                                      LABEL_PTR(add_l64_l64),
			                                      LABEL_PTR(add_l64_imm),

			                                      LABEL_PTR(sub_l64_l64),
			                                      LABEL_PTR(sub_l64_imm),

			                                      // LABEL_PTR(mul_l64_l64),
			                                      LABEL_PTR(mul_l64_imm),

			                                      LABEL_PTR(mod_l64_l64),
			                                      LABEL_PTR(mod_l64_imm),

			                                      LABEL_PTR(div_l64_imm),

			                                      LABEL_PTR(cmpEq_l64_l64),
			                                      LABEL_PTR(cmpEq_l64_imm),
			                                      LABEL_PTR(cmpG_l64_l64),
			                                      LABEL_PTR(cmpG_l64_imm),

			                                      LABEL_PTR(jmpRel_label),
			                                      LABEL_PTR(jmpRelIf_label),
			                                      LABEL_PTR(jmpRelNotIf_label),

			                                      LABEL_PTR(setPArg_l64),
			                                      LABEL_PTR(setPtrArg_lptr),
			                                      LABEL_PTR(call_func),

			                                      LABEL_PTR(ret_l64),
			                                      LABEL_PTR(ret_imm),

			                                      LABEL_PTR(init_type),
			                                      LABEL_PTR(deinit),

			                                      LABEL_PTR(input_l64),
			                                      LABEL_PTR(output_l64),

			                                      LABEL_PTR(nop),

			                                      LABEL_PTR(alloc_lptr_type),
			                                      LABEL_PTR(free_lptr),
			                                      LABEL_PTR(load_l64_lptr_ofs),
			                                      LABEL_PTR(store_lptr_l64_ofs) };
	#endif

		u64 r1 = 0;
		u64 r2 = 0;
		u64 r3 = 0;

		IF_NOT_CG(while (true)) {
			IF_CG(DISPATCH_OPCODE());

			if constexpr (!IGNORE_EXECUTION_STRATEGY) handleExecutionStrategyIfNeeded();

			IF_NOT_CG(switch (static_cast<OpcodeFix8>(function.bc[frame.instruction_pointer].opcode)
			)) {
				OP_CASE(mov_l64_imm)

				OP_CASE(mov_l64_l64)
				OP_CASE(cmov_l64_l64)

				OP_CASE(mov_l64_r0)
				OP_CASE(mov_r0_l64)

				OP_CASE(mov_l64_pFuncArg)
				OP_CASE(mov_lptr_ptrFuncArg)

				OP_CASE(add_l64_l64)
				OP_CASE(add_l64_imm)

				OP_CASE(sub_l64_l64)
				OP_CASE(sub_l64_imm)

				OP_CASE(mul_l64_imm)

				OP_CASE(mod_l64_imm)
				OP_CASE(mod_l64_l64)

				OP_CASE(div_l64_imm)

				OP_CASE(cmpEq_l64_l64)
				OP_CASE(cmpEq_l64_imm)
				OP_CASE(cmpG_l64_l64)
				OP_CASE(cmpG_l64_imm)

				OP_CASE(jmpRel_label)
				OP_CASE(jmpRelIf_label)
				OP_CASE(jmpRelNotIf_label)

				OP_CASE(setPtrArg_lptr)
				OP_CASE(setPArg_l64)
				OP_CASE(call_func)

				OP_CASE_END(ret_l64)
				OP_CASE_END(ret_imm)

				OP_CASE(init_type)
				OP_CASE(deinit)

				OP_CASE(input_l64)
				OP_CASE(output_l64)

				OP_CASE(nop)

				OP_CASE(alloc_lptr_type)
				OP_CASE(free_lptr)
				OP_CASE(load_l64_lptr_ofs)
				OP_CASE(store_lptr_l64_ofs)

				IF_NOT_CG(default
				          : {
							  // RIFT_PANIC("Unknown operator:", u64(instr.opcode));
						  })
			}
		}
	End:
		return frame.ret_val;
#else
		auto* instr = function.bc.data();
		return instr->opfun(instr, 0, 0, 0, reinterpret_cast<std::byte*>(local_stack), frame);
#endif
	}

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC pop_options
#endif

	void Executor::run(const Code& code) {
		// @TODO: ensure correct status

		setStatus(api::Running{});

		executing_code = &code;
		try {
			internalCallFunction(
				nullptr, executing_code->functions[code.main_id], { 0, memory.nullPtr() }
			);
			setStatus(api::NotStarted{});
		} catch (KillCoreException) { setStatus(api::NotStarted{}); }
	}

	void Executor::prestart() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped) {
			isRunning          = true;
			execution_strategy = ExecutionStrategy::Normal;
		}
	}

	void Executor::stop() {
		std::unique_lock lock(external_api_mutex);
		isRunning          = false;
		execution_strategy = ExecutionStrategy::Stoped;
		pause_cv.notify_all();
	}

	bool Executor::resume() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped
		    || execution_strategy == ExecutionStrategy::Normal) {
			return false;
		}
		isRunning          = true;
		execution_strategy = ExecutionStrategy::Normal;
		pause_cv.notify_all();
		return true;
	}

	bool Executor::pause() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped
		    || execution_strategy == ExecutionStrategy::Paused) {
			return false;
		} else if (execution_strategy == ExecutionStrategy::StepByStep) {
			return true;
		}
		isRunning          = false;
		execution_strategy = ExecutionStrategy::Paused;
		std::cout << "Set strategy to paused\n";
		return true;
	}

	bool Executor::step() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped)
			return false;
		else if (execution_strategy != ExecutionStrategy::Paused)
			return true;
		isRunning          = false;
		execution_strategy = ExecutionStrategy::StepByStep;
		pause_cv.notify_all();
		return true;
	}

	void Executor::setStatus(vm::api::ExecStatus new_status) {
		// @TODO: check if change is legal
		this->status = std::move(new_status);
		vcpu.onEvent(api::Executing{ status });
	}

	bool Executor::isPaused() {
		std::unique_lock lock(external_api_mutex);
		return execution_strategy == ExecutionStrategy::Paused;
	}

	bool Executor::isAlive() {
		std::unique_lock lock(external_api_mutex);
		return execution_strategy != ExecutionStrategy::Stoped;
	}

	void Executor::notifyPaused() { pause_cv.notify_all(); }
}  // namespace vm
