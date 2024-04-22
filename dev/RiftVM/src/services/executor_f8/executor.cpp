#include <base/ints.hpp>
#include <code_data/instruction.hpp>
#include <code_data/opcodes.hpp>
#include <code_data/code.hpp>
#include <cstring>
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
		if (is_running) return;
		handleExecutionStrategy();
	}

	[[gnu::always_inline]]
	inline void Executor::initNextFrame(Frame* frame, StandardFunctionArgs& args) {
		frame->regs      = Registers{ .p64_reg_0 = 0, .pointer_reg_0 = memory.nullPtr() };
		frame->flags     = FlagData{ .flag = false };
		frame->ret_val   = 0;
		frame->next_args = { 0, memory.nullPtr() };
		frame->args      = args;
		// if (!frame->block_id_stack.empty() || frame->local_stack_head != 0)
		//   RIFT_PANIC("init/deinits not paired");
	}

	Frame Executor::internalInitFrame() {
		return Frame{
			.instr       = nullptr,
			.local_stack = nullptr,
			.regs        = Registers{ .p64_reg_0 = 0, .pointer_reg_0 = memory.nullPtr() },
			.flags       = FlagData{ .flag = false },
			.ret_val     = 0,
			.next_args   = { 0, memory.nullPtr() },
			.args        = { 0, memory.nullPtr() },

			.block_id_stack   = std::vector<BlockId>(),
			.local_stack_head = 0,
		};
	}

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
	[[gnu::always_inline]]
	inline static T& derefStack(std::byte* stack, i64 position) {
		return *(reinterpret_cast<T*>(&stack[position]));
	}

	// NOTE: functions that implement opcodes (opfunctions) must be done this way:
	//
	// RETURN_TYPE OpFuns::op_<opcode_name>(OPFUN_ARGS) {
	//  {
	//    <function_body>
	//  }
	//  OPFUN_CONT(<step>);
	// }
	//
	// Function body must be seperated from the scope of OPFUN_CONT to make sure
	// that all its destructors have been called before invoking next tail call.
	// Otherwise, the compiler may get confused and may schedule destructors from
	// the body after the next tail call, which then becomes a regular function
	// call and may cause the stack to explode.

	RETURN_TYPE OpFuns::op_exit(OPFUN_ARGS) { IF_TC(return;) }

	RETURN_TYPE OpFuns::op_handle_strategy(OPFUN_ARGS) {
		{ executor.handleExecutionStrategy(); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmov_l64_l64(OPFUN_ARGS) {
		{
			if (frame->flags.flag)
				derefStack<u64>(local_stack, instr->arg0)
					= derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_l64_r0(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = frame->regs.p64_reg_0; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_r0_l64(OPFUN_ARGS) {
		{ frame->regs.p64_reg_0 = derefStack<u64>(local_stack, instr->arg0); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_l64_pFuncArg(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = frame->args.p64_arg; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_lptr_ptrFuncArg(OPFUN_ARGS) {
		{ derefStack<Pointer>(local_stack, instr->arg0) = frame->args.pointer_arg; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_add_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) += derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_add_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) += instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_sub_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) -= derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_sub_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) -= instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mul_l64_imm(OPFUN_ARGS) {
		{
			// @TODO: check types
			derefStack<u64>(local_stack, instr->arg0) *= instr->arg1;
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mod_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) %= instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mod_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) %= derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_div_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) /= instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmpEq_l64_l64(OPFUN_ARGS) {
		{
			frame->flags.flag = derefStack<u64>(local_stack, instr->arg0)
			                 == derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmpEq_l64_imm(OPFUN_ARGS) {
		{ frame->flags.flag = derefStack<u64>(local_stack, instr->arg0) == instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmpG_l64_l64(OPFUN_ARGS) {
		{
			frame->flags.flag = derefStack<u64>(local_stack, instr->arg0)
			                  > derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmpG_l64_imm(OPFUN_ARGS) {
		{ frame->flags.flag = derefStack<u64>(local_stack, instr->arg0) > instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_jmpRel_label(OPFUN_ARGS) {
		{ instr += instr->arg0; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_jmpRelIf_label(OPFUN_ARGS) {
		{
			if (frame->flags.flag) instr += instr->arg0;
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_jmpRelNotIf_label(OPFUN_ARGS) {
		{
			if (!frame->flags.flag) instr += instr->arg0;
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_setPtrArg_lptr(OPFUN_ARGS) {
		{ frame->next_args.pointer_arg = derefStack<Pointer>(local_stack, instr->arg0); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_setPArg_l64(OPFUN_ARGS) {
		{ frame->next_args.p64_arg = derefStack<i64>(local_stack, instr->arg0); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_call_func(OPFUN_ARGS) {
		{
			auto& runtime_data = executor.runtime_data;

			// Save current registers and flow.
			frame->instr       = instr + 1;
			frame->local_stack = local_stack;

			// Prepare new frame.
			auto* prev_frame = frame;
			frame++;
			i32 function_id = instr->arg0;
			if (frame + 1 > runtime_data.frame_stack_end) RIFT_PANIC("RiftVM stack overflow.");
			executor.initNextFrame(frame, prev_frame->next_args);

			// Update values passed as arguments.
			instr = executor.executing_code->functions[function_id].bc.data();

			u64 local_stack_size = executor.executing_code->functions[function_id].stack_size;
			local_stack          = runtime_data.local_stack_top;

			runtime_data.local_stack_top += local_stack_size;
			if (runtime_data.local_stack_top > runtime_data.local_stack_end)
				RIFT_PANIC("RiftVM stack overflow.");
			memset(local_stack, 0, local_stack_size);
		}
		OPFUN_CONT(0);
	}

	// @TODO: refactor op_rets to reduce code duplication

	RETURN_TYPE OpFuns::op_ret_l64(OPFUN_ARGS) {
		{
			u64 ret_val = derefStack<u64>(local_stack, instr->arg0);

			frame--;

			frame->regs.p64_reg_0                 = ret_val;
			executor.runtime_data.local_stack_top = local_stack;

			// Load previous frame
			instr       = frame->instr;  // This is already a pointer to next instr
			local_stack = frame->local_stack;
		}
		OPFUN_CONT(0);
	}

	RETURN_TYPE OpFuns::op_ret_imm(OPFUN_ARGS) {
		{
			frame--;

			frame->regs.p64_reg_0                 = instr->arg0;
			executor.runtime_data.local_stack_top = local_stack;

			// Load previous frame
			instr       = frame->instr;  // This is already a pointer to next instr
			local_stack = frame->local_stack;
		}
		OPFUN_CONT(0);
	}

	RETURN_TYPE OpFuns::op_init_type(OPFUN_ARGS) {
		{
			auto             type      = executor.types.getType(vm::TypeId(instr->arg0));
			auto             type_size = type->getSize();
			base::ModRawView data(&local_stack[frame->local_stack_head], type_size);
			frame->local_stack_head += type_size;
			auto block = executor.stack_allocator.makeTypeBlock(type, data);
			frame->block_id_stack.push_back(block);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_deinit(OPFUN_ARGS) {
		{
			auto block_id  = frame->block_id_stack.back();
			auto type_size = executor.memory.getBlock(block_id)->rawPointer().size();
			// if (type_size < frame->local_stack_head) RIFT_PANIC("init/deinits not paired");
			frame->local_stack_head -= type_size;
			executor.stack_allocator.deleteBlock(block_id);
			frame->block_id_stack.pop_back();
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_input_l64(OPFUN_ARGS) {
		{
			executor.setStatus(api::WaitingForInput{});
			derefStack<i64>(local_stack, instr->arg0) = executor.vcpu.getInput<i64>();
			executor.setStatus(api::Running{});
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_output_l64(OPFUN_ARGS) {
		{ executor.vcpu.writeOutput(derefStack<u64>(local_stack, instr->arg0)); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_nop(OPFUN_ARGS) { OPFUN_CONT(1); }

	RETURN_TYPE OpFuns::op_ext_l64(OPFUN_ARGS) {
		RIFT_PANIC("ext_l64 not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::op_alloc_lptr_type(OPFUN_ARGS) {
		{
			auto type = executor.types.getType(vm::TypeId(instr->arg1));
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
				block = executor.dynamic_allocator.makeArrayBlock(inner_type, table_size);
			} else {
				block = executor.dynamic_allocator.makeTypeBlock(type);
			}
			derefStack<Pointer>(local_stack, instr->arg0) = Pointer(block, 0);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_free_lptr(OPFUN_ARGS) {
		{
			executor.dynamic_allocator.deleteBlock(
				derefStack<Pointer>(local_stack, instr->arg0).getBlock()
			);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_load_l64_lptr_ofs(OPFUN_ARGS) {
		constexpr const uint8_t view_size = 8;
		int                     next      = 1;
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg1);
			auto view    = executor.internalDerefPointer(pointer);
			u64  idx     = 0;
#ifdef USE_TAIL_CALLS
			if (instr[1].opfun == OpFuns::op_ext_l64) [[likely]] {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#else
			if (static_cast<OpcodeFix8>(instr[1].opcode) == OpcodeFix8::ext_l64) [[likely]] {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#endif

			// @TODO: this assert degrades performance by 5-10%
			// RIFT_ASSERT(view.size() == view_size, "bad type");

			std::memcpy(&local_stack[instr->arg0], view.getBegin() + idx * view_size, view_size);
		}
		OPFUN_CONT(next);
	}

	RETURN_TYPE OpFuns::op_store_lptr_l64_ofs(OPFUN_ARGS) {
		constexpr const uint8_t view_size = 8;
		int                     next      = 1;
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg0);
			auto view    = executor.internalDerefPointer(pointer);
			u64  idx     = 0;
#ifdef USE_TAIL_CALLS
			if (instr[1].opfun == OpFuns::op_ext_l64) [[likely]] {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#else
			if (static_cast<OpcodeFix8>(instr[1].opcode) == OpcodeFix8::ext_l64) [[likely]] {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#endif

			// @TODO: this assert degrades performance by 5-10%
			// RIFT_ASSERT(view.size() == view_size, "bad type");

			std::memcpy(view.getBegin() + idx * view_size, &local_stack[instr->arg1], view_size);
		}
		OPFUN_CONT(next);
	}

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif

	u64 Executor::internalCallMain(const FuncData& main_func) {
		// We create one artificial "pre" frame, that when main function returns
		// it will go to it and end execution.
		Frame* pre_frame = runtime_data.frame_stack_base;

#ifdef USE_TAIL_CALLS
		Fix8Instruction exit_instr{ .opfun = OpFuns::op_exit, .arg0 = 0, .arg1 = 0 };
#else
		Fix8Instruction exit_instr{ .opcode = static_cast<u16>(OpcodeFix8::exit),
			                        .arg0   = 0,
			                        .arg1   = 0 };
#endif

		pre_frame->instr       = &exit_instr;
		pre_frame->local_stack = runtime_data.local_stack_top;

		// Frame of the main function.
		Frame*     frame       = runtime_data.frame_stack_base + 1;
		std::byte* local_stack = runtime_data.local_stack_top;
		runtime_data.local_stack_top += main_func.stack_size;

		auto* instr = main_func.bc.data();

#ifdef USE_TAIL_CALLS
		instr->opfun(instr, local_stack, frame, *this);
		return runtime_data.frame_stack_base->regs.p64_reg_0;
#else
	// Computed gotos labels:
	#ifdef USE_COMPUTED_GOTO
		constexpr static std::array<void*, OpCasesCount> opcode_label
			= { LABEL_PTR(mov_l64_imm),

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
			    LABEL_PTR(store_lptr_l64_ofs),

			    LABEL_PTR(ext_l64),
			    LABEL_PTR(exit) };
	#endif

		IF_NOT_CG(while (true)) {
			IF_CG(DISPATCH_OPCODE());

			if constexpr (!IGNORE_EXECUTION_STRATEGY) handleExecutionStrategyIfNeeded();

			IF_NOT_CG(switch (static_cast<OpcodeFix8>(instr->opcode))) {
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

				OP_CASE(ret_l64)
				OP_CASE(ret_imm)

				OP_CASE(init_type)
				OP_CASE(deinit)

				OP_CASE(input_l64)
				OP_CASE(output_l64)

				OP_CASE(nop)

				OP_CASE(alloc_lptr_type)
				OP_CASE(free_lptr)
				OP_CASE(load_l64_lptr_ofs)
				OP_CASE(store_lptr_l64_ofs)

				OP_CASE(ext_l64)
				OP_CASE_END(exit)

				IF_NOT_CG(default
				          : {
							  // RIFT_PANIC("Unknown operator:", u64(instr.opcode));
						  })
			}
		}
	End:
		return runtime_data.frame_stack_base->regs.p64_reg_0;
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
			internalCallMain(executing_code->functions[code.main_id]);
			setStatus(api::NotStarted{});
		} catch (KillCoreException) { setStatus(api::NotStarted{}); }
	}

	void Executor::prestart() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped) {
			is_running         = true;
			execution_strategy = ExecutionStrategy::Normal;
		}
	}

	void Executor::stop() {
		std::unique_lock lock(external_api_mutex);
		is_running         = false;
		execution_strategy = ExecutionStrategy::Stoped;
		pause_cv.notify_all();
	}

	bool Executor::resume() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped
		    || execution_strategy == ExecutionStrategy::Normal) {
			return false;
		}
		is_running         = true;
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
		is_running         = false;
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
		is_running         = false;
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
