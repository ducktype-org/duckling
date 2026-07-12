#pragma once

#include "safe_compiler.hpp"

#include <base/comptime/type_traits.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/low_program/utils.hpp>

#include <tuple>
#include <type_traits>

namespace vm::loader::compiler::safe::detail {
	/**
	 * @brief Checks whether a high-level instruction argument type can be translated
	 * to a specific low-level micro instruction argument type.
	 *
	 * A pair is valid when:
	 * 1) the high arg type is listed in `LowArg::ConstructibleFrom`, and
	 * 2) the high arg can be lowered by constructing `vm::opargs::OpCodeArg` from it
	 */
	template<typename LowArg, typename HighArg>
	concept IsTranslatableInstructionArgumentPair
		= base::IsTupleMember<std::remove_cvref_t<HighArg>, typename LowArg::ConstructibleFrom>
	   && std::constructible_from<vm::opargs::OpCodeArg, std::remove_cvref_t<HighArg>>;

	/**
	 * @brief Type-level validation of translation for full argument lists.
	 */
	template<typename LowArgsTuple, typename... HighArgs>
	struct AreTranslatableInstructionArgumentLists: std::false_type {};

	template<typename... LowArgs, typename... HighArgs>
	struct AreTranslatableInstructionArgumentLists<std::tuple<LowArgs...>, HighArgs...>:
		  std::bool_constant<
			  (sizeof...(LowArgs) == sizeof...(HighArgs))
			  && (IsTranslatableInstructionArgumentPair<LowArgs, HighArgs> && ...)> {};

	/**
	 * @brief Validates that a micro-instruction tag `T` can be constructed from `Args...`
	 * at lowering call sites.
	 *
	 * This checks both argument count and per-position source compatibility declared
	 * by low arg types.
	 */
	template<typename T, typename... Args>
	concept AreTranslatableInstructionTagArgs
		= vm::low::instruction_tags::IsMicroInstructionTag<T>
	   && AreTranslatableInstructionArgumentLists<typename T::ArgTypes, Args...>::value;

	namespace high = vm::code::instructions;
	using namespace vm::low::instruction_tags;

	/**
	 * Helper class for lowering high bytecode instructions.
	 * This class simply holds all the relevant context and defines some helper methods, which make
	 * defining instruction lowering recipes clean and succinct.
	 * When adding a new instruction simply add a new switch branch in `MicroBytecodeBuilder::add`.
	 *
	 * Beside generating a vector of `MicroInstruction`s, this class also provides a map
	 * from temporary label IDs to label offsets used later by `Compiler::linkLabelArguments`.
	 */
	class SafeMicroBytecodeBuilder {
		safe::SafeCompiler&                                       compiler;
		const vm::loader::compiler::detail::FunctionStackContext& ctx;

		base::HashMap<usize, usize>       label_id_to_offset{};
		base::HashMap<base::StrID, usize> label_name_to_id{};
		usize                             next_instruction_index = 0;

		low::MicroBytecode result;

#if (BUILD_TYPE_DEV_DEBUG)
		std::string current_high_instruction_representation{};
#endif

	public:
		code::StackStateID curr_state = code::LocalStackDb::EMPTY;

		SafeMicroBytecodeBuilder(
			safe::SafeCompiler&                                       compiler,
			const vm::loader::compiler::detail::FunctionStackContext& ctx
		):
			  compiler{ compiler },
			  ctx{ ctx }
#ifdef ENABLE_JIT
			  ,
			  next_instruction_index{ 1 },
			  result{ makeLowInstruction(vm::low::MicroOpcode::jitFuncEntrypoint, 0, 0) }
#endif
		{
		}

		std::pair<low::MicroBytecode, decltype(label_id_to_offset)> build() {
			return { std::move(result), std::move(label_id_to_offset) };
		}

		/**
		 * @brief Add a new high instruction.
		 * @return InstructionRange of the added instruction.
		 * The end index is exclusive, so the instruction occupies the range [begin, end).
		 */
		low::LowFuncData::InstructionRange add(const code::Instruction& instruction);


	private:
		/**
		 * @brief Whether to add a step Gil instruction before the next low instruction.
		 */
		bool push_step_gil_on_next_add_low = true;
		bool is_control_flow               = true;

		TypeCRef getPlaceType(const opargs::ArgumentType auto p) const {
			if (auto maybe_val = ctx.function.local_stack.getTypeName(curr_state, p.var_name)) {
				code::valid_type::ValidTypeID type_id = compiler.high_program.getTypeContext()
				                                            .getCurrentTypes()
				                                            .at(*maybe_val)
				                                            ->getID();
				return compiler.getLowProgram()->getTypes().at(TypeID(type_id.asInt()));
			}
			return compiler.getLowProgram()->getGlobals().at(p.var_name)->type;
		}

		template<typename LowArg, typename HighArg>
		requires IsTranslatableInstructionArgumentPair<LowArg, HighArg>
		u64 lowerLowArg(const HighArg& arg) {
			if constexpr (std::constructible_from<u64, HighArg>) {
				return u64(arg);
			} else {
				return compiler.template lowerArgument<std::remove_cvref_t<HighArg>, LowArg>(
					ctx, label_name_to_id, arg, curr_state
				);
			}
		}

		template<typename T, typename... Args>
		requires AreTranslatableInstructionTagArgs<T, Args...> void addLow(Args&&... args) {
			if (push_step_gil_on_next_add_low) {
				push_step_gil_on_next_add_low = false;
				addLow<Op_stepGil>();
			}

			if (is_control_flow) {
				is_control_flow = false;
				addLow<Op_check_strategy>();
			}

			[&]<typename... LowArgs>(std::tuple<LowArgs...>*) {
				result.push_back(
					makeLowInstruction(T::OPCODE, lowerLowArg<LowArgs>(std::forward<Args>(args))...)
				);
#if (BUILD_TYPE_DEV_DEBUG)
				result.back().opcode_id      = T::OPCODE;
				result.back().representation = current_high_instruction_representation;
#endif
				next_instruction_index++;
			}(static_cast<T::ArgTypes*>(nullptr));
		}

		void addLabel(opargs::Label label) {
			usize lid = compiler.lowerArgument<opargs::Label, low::opargs::Label>(
				ctx, label_name_to_id, label, curr_state
			);
			label_id_to_offset.put(lid, next_instruction_index);
		}
	};

	low::LowFuncData::InstructionRange SafeMicroBytecodeBuilder::add(
		const code::Instruction& instruction
	) {
#if (BUILD_TYPE_DEV_DEBUG)
		current_high_instruction_representation = code::instructionToString(instruction);
#endif
		usize instruction_begin_index = next_instruction_index;

		push_step_gil_on_next_add_low = true;

		// Mark control flow instruction
		// @TODO: #2692 make it an instruction's trait
		PUSH_DIAGNOSTIC
		UNHANDLED_ENUM
		instr_match(instruction) {
			instr_case(high::Op_jmp_label, _) { is_control_flow = true; }
			instr_case(high::Op_jmpIf_label, _) { is_control_flow = true; }
			instr_case(high::Op_jmpIfNot_label, _) { is_control_flow = true; }
			instr_case(high::Op_call_builtinfunc, _) { is_control_flow = true; }
			instr_case(high::Op_call_cfunc, _) { is_control_flow = true; }
			instr_case(high::Op_call_ffifunc, _) { is_control_flow = true; }
			instr_case(high::Op_call_func, _) { is_control_flow = true; }
			instr_case(high::Op_virtual_call_pptr_method, _) { is_control_flow = true; }
			instr_default { is_control_flow = false; }
		}
		POP_DIAGNOSTIC


		PUSH_DIAGNOSTIC
		UNHANDLED_ENUM
		instr_match(instruction) {
			instr_case(high::Op_mov_p8_imm, i) { addLow<Op_mov_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_p8_p8, i) { addLow<Op_mov_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_cmov_p8_p8, i) { addLow<Op_cmov_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_cmov_p8_imm, i) { addLow<Op_cmov_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_p16_imm, i) { addLow<Op_mov_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_p16_p16, i) { addLow<Op_mov_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_cmov_p16_p16, i) { addLow<Op_cmov_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_cmov_p16_imm, i) { addLow<Op_cmov_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_p32_imm, i) { addLow<Op_mov_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_p32_p32, i) { addLow<Op_mov_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_cmov_p32_p32, i) { addLow<Op_cmov_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_cmov_p32_imm, i) { addLow<Op_cmov_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_p64_imm, i) { addLow<Op_mov_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_p64_p64, i) { addLow<Op_mov_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_cmov_p64_p64, i) { addLow<Op_cmov_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_cmov_p64_imm, i) { addLow<Op_cmov_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_pptr_pptr, i) { addLow<Op_mov_pptr_pptr>(i.dst, i.src); }
			instr_case(high::Op_setNull_pptr, i) { addLow<Op_setNull_pptr>(i.dst); }
			instr_case(high::Op_mov_popq_popq, i) {
				auto type_size = getPlaceType(i.src)->getSize().asInt();
				addLow<Op_mov_popq_popq>(i.dst, i.src);
				addLow<Op_ext_imm>(vm::opargs::Immediate{ type_size });
			}
			instr_case(high::Op_mov_pste_pste, i) { addLow<Op_mov_bste_bste>(i.dst, i.src); }
			instr_case(high::Op_mov_pfst_pfst, i) { addLow<Op_mov_bfst_bfst>(i.dst, i.src); }
			instr_case(high::Op_add_p64_p64, i) { addLow<Op_add_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_add_p64_imm, i) { addLow<Op_add_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_add_p32_p32, i) { addLow<Op_add_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_add_p32_imm, i) { addLow<Op_add_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_add_p16_p16, i) { addLow<Op_add_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_add_p16_imm, i) { addLow<Op_add_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_add_p8_p8, i) { addLow<Op_add_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_add_p8_imm, i) { addLow<Op_add_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_sub_p64_p64, i) { addLow<Op_sub_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_sub_p64_imm, i) { addLow<Op_sub_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_sub_p32_p32, i) { addLow<Op_sub_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_sub_p32_imm, i) { addLow<Op_sub_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_sub_p16_p16, i) { addLow<Op_sub_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_sub_p16_imm, i) { addLow<Op_sub_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_sub_p8_p8, i) { addLow<Op_sub_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_sub_p8_imm, i) { addLow<Op_sub_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_mul_p64_p64, i) { addLow<Op_mul_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_mul_p64_imm, i) { addLow<Op_mul_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_mul_p32_p32, i) { addLow<Op_mul_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_mul_p32_imm, i) { addLow<Op_mul_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_mul_p16_p16, i) { addLow<Op_mul_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_mul_p16_imm, i) { addLow<Op_mul_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_mul_p8_p8, i) { addLow<Op_mul_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_mul_p8_imm, i) { addLow<Op_mul_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_div_p64_p64, i) { addLow<Op_div_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_div_p64_imm, i) { addLow<Op_div_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_div_p32_p32, i) { addLow<Op_div_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_div_p32_imm, i) { addLow<Op_div_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_div_p16_p16, i) { addLow<Op_div_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_div_p16_imm, i) { addLow<Op_div_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_div_p8_p8, i) { addLow<Op_div_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_div_p8_imm, i) { addLow<Op_div_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_mod_p64_p64, i) { addLow<Op_mod_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_mod_p64_imm, i) { addLow<Op_mod_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_mod_p32_p32, i) { addLow<Op_mod_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_mod_p32_imm, i) { addLow<Op_mod_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_mod_p16_p16, i) { addLow<Op_mod_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_mod_p16_imm, i) { addLow<Op_mod_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_mod_p8_p8, i) { addLow<Op_mod_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_mod_p8_imm, i) { addLow<Op_mod_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_neg_p64, i) { addLow<Op_neg_p64>(i.dst); }
			instr_case(high::Op_neg_p32, i) { addLow<Op_neg_p32>(i.dst); }
			instr_case(high::Op_neg_p16, i) { addLow<Op_neg_p16>(i.dst); }
			instr_case(high::Op_neg_p8, i) { addLow<Op_neg_p8>(i.dst); }
			instr_case(high::Op_fadd_p64_p64, i) { addLow<Op_fadd_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_fadd_p64_imm, i) { addLow<Op_fadd_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_fadd_p32_p32, i) { addLow<Op_fadd_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_fadd_p32_imm, i) { addLow<Op_fadd_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_fsub_p64_p64, i) { addLow<Op_fsub_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_fsub_p64_imm, i) { addLow<Op_fsub_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_fsub_p32_p32, i) { addLow<Op_fsub_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_fsub_p32_imm, i) { addLow<Op_fsub_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_fmul_p64_p64, i) { addLow<Op_fmul_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_fmul_p64_imm, i) { addLow<Op_fmul_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_fmul_p32_p32, i) { addLow<Op_fmul_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_fmul_p32_imm, i) { addLow<Op_fmul_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_fdiv_p64_p64, i) { addLow<Op_fdiv_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_fdiv_p64_imm, i) { addLow<Op_fdiv_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_fdiv_p32_p32, i) { addLow<Op_fdiv_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_fdiv_p32_imm, i) { addLow<Op_fdiv_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_fneg_p64, i) { addLow<Op_fneg_p64>(i.dst); }
			instr_case(high::Op_fneg_p32, i) { addLow<Op_fneg_p32>(i.dst); }
			instr_case(high::Op_umul_p64_p64, i) { addLow<Op_umul_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_umul_p64_imm, i) { addLow<Op_umul_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_umul_p32_p32, i) { addLow<Op_umul_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_umul_p32_imm, i) { addLow<Op_umul_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_umul_p16_p16, i) { addLow<Op_umul_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_umul_p16_imm, i) { addLow<Op_umul_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_umul_p8_p8, i) { addLow<Op_umul_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_umul_p8_imm, i) { addLow<Op_umul_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_umod_p64_p64, i) { addLow<Op_umod_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_umod_p64_imm, i) { addLow<Op_umod_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_umod_p32_p32, i) { addLow<Op_umod_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_umod_p32_imm, i) { addLow<Op_umod_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_umod_p16_p16, i) { addLow<Op_umod_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_umod_p16_imm, i) { addLow<Op_umod_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_umod_p8_p8, i) { addLow<Op_umod_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_umod_p8_imm, i) { addLow<Op_umod_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_udiv_p64_p64, i) { addLow<Op_udiv_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_udiv_p64_imm, i) { addLow<Op_udiv_p64_imm>(i.dst, i.src); }
			instr_case(high::Op_udiv_p32_p32, i) { addLow<Op_udiv_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_udiv_p32_imm, i) { addLow<Op_udiv_p32_imm>(i.dst, i.src); }
			instr_case(high::Op_udiv_p16_p16, i) { addLow<Op_udiv_p16_p16>(i.dst, i.src); }
			instr_case(high::Op_udiv_p16_imm, i) { addLow<Op_udiv_p16_imm>(i.dst, i.src); }
			instr_case(high::Op_udiv_p8_p8, i) { addLow<Op_udiv_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_udiv_p8_imm, i) { addLow<Op_udiv_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_log_and_p8_p8, i) { addLow<Op_log_and_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_log_and_p8_imm, i) { addLow<Op_log_and_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_log_or_p8_p8, i) { addLow<Op_log_or_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_log_or_p8_imm, i) { addLow<Op_log_or_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_log_xor_p8_p8, i) { addLow<Op_log_xor_p8_p8>(i.dst, i.src); }
			instr_case(high::Op_log_xor_p8_imm, i) { addLow<Op_log_xor_p8_imm>(i.dst, i.src); }
			instr_case(high::Op_log_not_p8, i) { addLow<Op_log_not_p8>(i.dst); }
			instr_case(high::Op_cmpEq_p64_p64, i) { addLow<Op_cmpEq_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_p64_imm, i) { addLow<Op_cmpEq_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_p64_p64, i) { addLow<Op_cmpNeq_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_p64_imm, i) { addLow<Op_cmpNeq_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_p64_p64, i) { addLow<Op_cmpGt_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_p64_imm, i) { addLow<Op_cmpGt_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_p64_p64, i) { addLow<Op_cmpGe_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_p64_imm, i) { addLow<Op_cmpGe_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_p64_p64, i) { addLow<Op_ucmpGt_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_p64_imm, i) { addLow<Op_ucmpGt_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_p64_p64, i) { addLow<Op_ucmpGe_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_p64_imm, i) { addLow<Op_ucmpGe_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_p64_p64, i) { addLow<Op_cmpLt_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_p64_imm, i) { addLow<Op_cmpLt_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_p64_p64, i) { addLow<Op_cmpLe_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_p64_imm, i) { addLow<Op_cmpLe_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_p64_p64, i) { addLow<Op_ucmpLt_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_p64_imm, i) { addLow<Op_ucmpLt_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_p64_p64, i) { addLow<Op_ucmpLe_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_p64_imm, i) { addLow<Op_ucmpLe_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_p32_p32, i) { addLow<Op_cmpEq_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_p32_imm, i) { addLow<Op_cmpEq_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_p32_p32, i) { addLow<Op_cmpNeq_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_p32_imm, i) { addLow<Op_cmpNeq_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_p32_p32, i) { addLow<Op_cmpGt_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_p32_imm, i) { addLow<Op_cmpGt_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_p32_p32, i) { addLow<Op_cmpGe_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_p32_imm, i) { addLow<Op_cmpGe_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_p32_p32, i) { addLow<Op_ucmpGt_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_p32_imm, i) { addLow<Op_ucmpGt_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_p32_p32, i) { addLow<Op_ucmpGe_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_p32_imm, i) { addLow<Op_ucmpGe_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_p32_p32, i) { addLow<Op_cmpLt_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_p32_imm, i) { addLow<Op_cmpLt_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_p32_p32, i) { addLow<Op_cmpLe_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_p32_imm, i) { addLow<Op_cmpLe_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_p32_p32, i) { addLow<Op_ucmpLt_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_p32_imm, i) { addLow<Op_ucmpLt_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_p32_p32, i) { addLow<Op_ucmpLe_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_p32_imm, i) { addLow<Op_ucmpLe_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_p16_p16, i) { addLow<Op_cmpEq_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_p16_imm, i) { addLow<Op_cmpEq_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_p16_p16, i) { addLow<Op_cmpNeq_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_p16_imm, i) { addLow<Op_cmpNeq_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_p16_p16, i) { addLow<Op_cmpGt_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_p16_imm, i) { addLow<Op_cmpGt_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_p16_p16, i) { addLow<Op_cmpGe_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_p16_imm, i) { addLow<Op_cmpGe_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_p16_p16, i) { addLow<Op_ucmpGt_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_p16_imm, i) { addLow<Op_ucmpGt_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_p16_p16, i) { addLow<Op_ucmpGe_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_p16_imm, i) { addLow<Op_ucmpGe_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_p16_p16, i) { addLow<Op_cmpLt_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_p16_imm, i) { addLow<Op_cmpLt_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_p16_p16, i) { addLow<Op_cmpLe_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_p16_imm, i) { addLow<Op_cmpLe_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_p16_p16, i) { addLow<Op_ucmpLt_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_p16_imm, i) { addLow<Op_ucmpLt_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_p16_p16, i) { addLow<Op_ucmpLe_p16_p16>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_p16_imm, i) { addLow<Op_ucmpLe_p16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_p8_p8, i) { addLow<Op_cmpEq_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_p8_imm, i) { addLow<Op_cmpEq_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_p8_p8, i) { addLow<Op_cmpNeq_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_p8_imm, i) { addLow<Op_cmpNeq_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_p8_p8, i) { addLow<Op_cmpGt_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_p8_imm, i) { addLow<Op_cmpGt_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_p8_p8, i) { addLow<Op_cmpGe_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_p8_imm, i) { addLow<Op_cmpGe_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_p8_p8, i) { addLow<Op_ucmpGt_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_p8_imm, i) { addLow<Op_ucmpGt_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_p8_p8, i) { addLow<Op_ucmpGe_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_p8_imm, i) { addLow<Op_ucmpGe_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_p8_p8, i) { addLow<Op_cmpLt_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_p8_imm, i) { addLow<Op_cmpLt_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_p8_p8, i) { addLow<Op_cmpLe_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_p8_imm, i) { addLow<Op_cmpLe_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_p8_p8, i) { addLow<Op_ucmpLt_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_p8_imm, i) { addLow<Op_ucmpLt_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_p8_p8, i) { addLow<Op_ucmpLe_p8_p8>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_p8_imm, i) { addLow<Op_ucmpLe_p8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpEq_p64_p64, i) { addLow<Op_fcmpEq_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpEq_p64_imm, i) { addLow<Op_fcmpEq_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpNeq_p64_p64, i) { addLow<Op_fcmpNeq_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpNeq_p64_imm, i) { addLow<Op_fcmpNeq_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGt_p64_p64, i) { addLow<Op_fcmpGt_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGt_p64_imm, i) { addLow<Op_fcmpGt_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGe_p64_p64, i) { addLow<Op_fcmpGe_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGe_p64_imm, i) { addLow<Op_fcmpGe_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLt_p64_p64, i) { addLow<Op_fcmpLt_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLt_p64_imm, i) { addLow<Op_fcmpLt_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLe_p64_p64, i) { addLow<Op_fcmpLe_p64_p64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLe_p64_imm, i) { addLow<Op_fcmpLe_p64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpEq_p32_p32, i) { addLow<Op_fcmpEq_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpEq_p32_imm, i) { addLow<Op_fcmpEq_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpNeq_p32_p32, i) { addLow<Op_fcmpNeq_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpNeq_p32_imm, i) { addLow<Op_fcmpNeq_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGt_p32_p32, i) { addLow<Op_fcmpGt_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGt_p32_imm, i) { addLow<Op_fcmpGt_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGe_p32_p32, i) { addLow<Op_fcmpGe_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGe_p32_imm, i) { addLow<Op_fcmpGe_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLt_p32_p32, i) { addLow<Op_fcmpLt_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLt_p32_imm, i) { addLow<Op_fcmpLt_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLe_p32_p32, i) { addLow<Op_fcmpLe_p32_p32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLe_p32_imm, i) { addLow<Op_fcmpLe_p32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNull_pptr, i) { addLow<Op_cmpNull_pptr>(i.ptr); }
			instr_case(high::Op_variantSetInner_pvnt_type, i) {
				addLow<Op_variantSetInner_bvnt_type>(i.variant, i.inner_type);
				opargs::Type variant_type = getPlaceType(i.variant)->getName();
				addLow<Op_ext_type>(variant_type);
			}
			instr_case(high::Op_variantGetInner_pptr_pvnt_type, i) {
				addLow<Op_variantGetInner_pptr_bvnt>(i.dst_ptr, i.variant);
				opargs::Type variant_type = getPlaceType(i.variant)->getName();
				addLow<Op_ext_type_type>(i.expected_type, variant_type);
			}
			instr_case(high::Op_variantSetInner_pptr_type, i) {
				addLow<Op_variantSetInner_pptr_type>(i.variant_ptr, i.inner_type);
				opargs::Type variant_type
					= getPlaceType(i.variant_ptr)->getInnerType().value()->getName();
				addLow<Op_ext_type>(variant_type);
			}
			instr_case(high::Op_variantGetInner_pptr_pptr_type, i) {
				addLow<Op_variantGetInner_pptr_pptr>(i.dst_ptr, i.variant_ptr);
				opargs::Type variant_type
					= getPlaceType(i.variant_ptr)->getInnerType().value()->getName();
				addLow<Op_ext_type_type>(i.expected_type, variant_type);
			}
			instr_case(high::Op_label, i) { addLabel(i.label); }
			instr_case(high::Op_jmp_label, i) { addLow<Op_jmp_label>(i.label); }
			instr_case(high::Op_jmpIf_label, i) { addLow<Op_jmpIf_label>(i.label); }
			instr_case(high::Op_jmpIfNot_label, i) { addLow<Op_jmpIfNot_label>(i.label); }
			instr_case(high::Op_call_func, i) { addLow<Op_call_func>(i.function); }
			instr_case(high::Op_call_builtinfunc, i) { addLow<Op_call_builtinfunc>(i.function); }
			instr_case(high::Op_call_cfunc, i) { addLow<Op_call_cfunc>(i.function); }
			instr_case(high::Op_call_ffifunc, i) { addLow<Op_call_ffifunc>(i.function); }
			instr_case(high::Op_set_threadctx, i) { addLow<Op_set_threadctx>(i.function); }
			instr_case(high::Op_ret_tailcall_func, i) { addLow<Op_ret_tailcall_func>(i.function); }
			instr_case(high::Op_ret, i) { addLow<Op_ret>(); }
			instr_case(high::Op_init_pany_type, i) { addLow<Op_init_bany_type>(i.var, i.type); }
			instr_case(high::Op_deinit, i) { addLow<Op_deinit>(); }
			instr_case(high::Op_input_p64, i) { addLow<Op_input_p64>(i.dst); }
			instr_case(high::Op_output_p64, i) { addLow<Op_output_p64>(i.src); }
			instr_case(high::Op_input_p32, i) { addLow<Op_input_p32>(i.dst); }
			instr_case(high::Op_output_p32, i) { addLow<Op_output_p32>(i.src); }
			instr_case(high::Op_setVTable_pptr_type, i) {
				addLow<Op_setVTable_pptr_type>(i.object_ptr, i.type);
			}
			instr_case(high::Op_resetVTable_pptr, i) { addLow<Op_resetVTable_pptr>(i.object_ptr); }
			instr_case(high::Op_upcast_pptr_pptr, i) { addLow<Op_mov_pptr_pptr>(i.dst, i.src); }
			instr_case(high::Op_downcast_pptr_pptr, i) {
				addLow<Op_downcast_pptr_pptr>(i.dst, i.src);
				opargs::Type variant_type = getPlaceType(i.dst)->getInnerType().value()->getName();
				addLow<Op_ext_type>(variant_type);
			}
			instr_case(high::Op_virtual_call_pptr_method, i) {
				addLow<Op_virtual_call_pptr_method>(i.object_ptr, i.method);
			}
			instr_case(high::Op_alloc_pptr_type, i) { addLow<Op_alloc_pptr_type>(i.ptr, i.type); }
			instr_case(high::Op_free_pptr, i) { addLow<Op_free_pptr>(i.ptr); }
			instr_case(high::Op_store_pptr_pany, i) {
				addLow<Op_store_pptr_bany>(i.dst_ptr, i.src);
			}
			instr_case(high::Op_load_pany_pptr, i) { addLow<Op_load_bany_pptr>(i.dst, i.src_ptr); }
			instr_case(high::Op_ref_pptr_pany, i) { addLow<Op_ref_pptr_bany>(i.dst_ptr, i.src); }
			instr_case(high::Op_ref_pptr_pvnt, i) { addLow<Op_ref_pptr_bany>(i.dst_ptr, i.src); }
			instr_case(high::Op_structLea_pptr_pptr_field, i) {
				addLow<Op_structLea_pptr_pptr>(i.dst_ptr, i.src_data_ptr);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structLoad_pany_pptr_field, i) {
				addLow<Op_structLoad_bany_pptr>(i.dst, i.src_data_ptr);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structStore_pptr_pany_field, i) {
				addLow<Op_structStore_pptr_bany>(i.dst_data_ptr, i.src);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structLea_pptr_pste_field, i) {
				addLow<Op_structLea_pptr_bste>(i.dst_ptr, i.src_data_struct);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structLoad_pany_pste_field, i) {
				addLow<Op_structLoad_bany_bste>(i.dst, i.src_data_struct);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structStore_pste_pany_field, i) {
				addLow<Op_structStore_bste_bany>(i.dst_data_struct, i.src);
				addLow<Op_ext_field>(i.field);
			}
#define TABLE_PTR_ELEM_TYPE(ptr_arg) \
	(*(*getPlaceType(ptr_arg)->getInnerType())->getInnerType())->getName()
#define TABLE_VAL_ELEM_TYPE(val_arg) (*getPlaceType(val_arg)->getInnerType())->getName()
			instr_case(high::Op_fixedSizeTableLea_pptr_pptr_p64, i) {
				addLow<Op_anyArrayLea_pptr_pptr>(i.dst_ptr, i.src_table_ptr);
				addLow<Op_ext_p64_type>(
					i.index, opargs::Type{ TABLE_PTR_ELEM_TYPE(i.src_table_ptr) }
				);
			}
			instr_case(high::Op_fixedSizeTableLoad_pany_pptr_p64, i) {
				addLow<Op_anyArrayLoad_bany_pptr>(i.dst, i.src_table_ptr);
				addLow<Op_ext_p64_type>(
					i.index, opargs::Type{ TABLE_PTR_ELEM_TYPE(i.src_table_ptr) }
				);
			}
			instr_case(high::Op_fixedSizeTableStore_pptr_pany_p64, i) {
				addLow<Op_anyArrayStore_pptr_bany>(i.dst_table_ptr, i.src);
				addLow<Op_ext_p64_type>(
					i.index, opargs::Type{ TABLE_PTR_ELEM_TYPE(i.dst_table_ptr) }
				);
			}
			instr_case(high::Op_fixedSizeTableLea_pptr_pfst_p64, i) {
				addLow<Op_fixedSizeTableLea_pptr_bfst>(i.dst_ptr, i.src_table);
				addLow<Op_ext_p64_type>(i.index, opargs::Type{ TABLE_VAL_ELEM_TYPE(i.src_table) });
			}
			instr_case(high::Op_fixedSizeTableLoad_pany_pfst_p64, i) {
				addLow<Op_fixedSizeTableLoad_bany_bfst>(i.dst, i.src_table);
				addLow<Op_ext_p64_type>(i.index, opargs::Type{ TABLE_VAL_ELEM_TYPE(i.src_table) });
			}
			instr_case(high::Op_fixedSizeTableStore_pfst_pany_p64, i) {
				addLow<Op_fixedSizeTableStore_bfst_bany>(i.dst_table, i.src);
				addLow<Op_ext_p64_type>(i.index, opargs::Type{ TABLE_VAL_ELEM_TYPE(i.dst_table) });
			}
			instr_case(high::Op_dynTableLea_pptr_pptr_p64, i) {
				addLow<Op_anyArrayLea_pptr_pptr>(i.dst_ptr, i.src_table_ptr);
				addLow<Op_ext_p64_type>(
					i.index, opargs::Type{ TABLE_PTR_ELEM_TYPE(i.src_table_ptr) }
				);
			}
			instr_case(high::Op_dynTableLoad_pany_pptr_p64, i) {
				addLow<Op_anyArrayLoad_bany_pptr>(i.dst, i.src_table_ptr);
				addLow<Op_ext_p64_type>(
					i.index, opargs::Type{ TABLE_PTR_ELEM_TYPE(i.src_table_ptr) }
				);
			}
			instr_case(high::Op_dynTableStore_pptr_pany_p64, i) {
				addLow<Op_anyArrayStore_pptr_bany>(i.dst_table_ptr, i.src);
				addLow<Op_ext_p64_type>(
					i.index, opargs::Type{ TABLE_PTR_ELEM_TYPE(i.dst_table_ptr) }
				);
			}
			instr_case(high::Op_dynTableReAlloc_pptr_type_p64, i) {
				addLow<Op_dynTableReAlloc_pptr_type>(i.dst_table_ptr, i.table_type);
				addLow<Op_ext_p64>(i.new_elem_count);
			}
#undef TABLE_PTR_ELEM_TYPE
#undef TABLE_VAL_ELEM_TYPE
			instr_case(high::Op_strOutput_pptr, i) { addLow<Op_strOutput_pptr>(i.string_ptr); }
			instr_case(high::Op_cast_p8_type, i) {}
			instr_case(high::Op_cast_p16_type, i) {}
			instr_case(high::Op_cast_p32_type, i) {}
			instr_case(high::Op_cast_p64_type, i) {}
			instr_case(high::Op_fstToDynTable_pptr_pptr, i) {
				addLow<Op_mov_pptr_pptr>(i.dst_table_ptr, i.src_table_ptr);
			}

			// Sign Extension
			instr_case(high::Op_sext_p16_p8, i) { addLow<Op_sext_p16_p8>(i.dst, i.src); }
			instr_case(high::Op_sext_p32_p8, i) { addLow<Op_sext_p32_p8>(i.dst, i.src); }
			instr_case(high::Op_sext_p64_p8, i) { addLow<Op_sext_p64_p8>(i.dst, i.src); }
			instr_case(high::Op_sext_p32_p16, i) { addLow<Op_sext_p32_p16>(i.dst, i.src); }
			instr_case(high::Op_sext_p64_p16, i) { addLow<Op_sext_p64_p16>(i.dst, i.src); }
			instr_case(high::Op_sext_p64_p32, i) { addLow<Op_sext_p64_p32>(i.dst, i.src); }

			// Zero Extension
			instr_case(high::Op_zext_p16_p8, i) { addLow<Op_zext_p16_p8>(i.dst, i.src); }
			instr_case(high::Op_zext_p32_p8, i) { addLow<Op_zext_p32_p8>(i.dst, i.src); }
			instr_case(high::Op_zext_p64_p8, i) { addLow<Op_zext_p64_p8>(i.dst, i.src); }
			instr_case(high::Op_zext_p32_p16, i) { addLow<Op_zext_p32_p16>(i.dst, i.src); }
			instr_case(high::Op_zext_p64_p16, i) { addLow<Op_zext_p64_p16>(i.dst, i.src); }
			instr_case(high::Op_zext_p64_p32, i) { addLow<Op_zext_p64_p32>(i.dst, i.src); }

			// Truncation
			instr_case(high::Op_trunc_p8_p16, i) { addLow<Op_trunc_p8_p16>(i.dst, i.src); }
			instr_case(high::Op_trunc_p8_p32, i) { addLow<Op_trunc_p8_p32>(i.dst, i.src); }
			instr_case(high::Op_trunc_p8_p64, i) { addLow<Op_trunc_p8_p64>(i.dst, i.src); }
			instr_case(high::Op_trunc_p16_p32, i) { addLow<Op_trunc_p16_p32>(i.dst, i.src); }
			instr_case(high::Op_trunc_p16_p64, i) { addLow<Op_trunc_p16_p64>(i.dst, i.src); }
			instr_case(high::Op_trunc_p32_p64, i) { addLow<Op_trunc_p32_p64>(i.dst, i.src); }

			// Int to Float
			instr_case(high::Op_sitofp_p32_p8, i) { addLow<Op_sitofp_p32_p8>(i.dst, i.src); }
			instr_case(high::Op_uitofp_p32_p8, i) { addLow<Op_uitofp_p32_p8>(i.dst, i.src); }
			instr_case(high::Op_sitofp_p32_p16, i) { addLow<Op_sitofp_p32_p16>(i.dst, i.src); }
			instr_case(high::Op_uitofp_p32_p16, i) { addLow<Op_uitofp_p32_p16>(i.dst, i.src); }
			instr_case(high::Op_sitofp_p32_p32, i) { addLow<Op_sitofp_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_uitofp_p32_p32, i) { addLow<Op_uitofp_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_sitofp_p32_p64, i) { addLow<Op_sitofp_p32_p64>(i.dst, i.src); }
			instr_case(high::Op_uitofp_p32_p64, i) { addLow<Op_uitofp_p32_p64>(i.dst, i.src); }
			instr_case(high::Op_sitofp_p64_p8, i) { addLow<Op_sitofp_p64_p8>(i.dst, i.src); }
			instr_case(high::Op_uitofp_p64_p8, i) { addLow<Op_uitofp_p64_p8>(i.dst, i.src); }
			instr_case(high::Op_sitofp_p64_p16, i) { addLow<Op_sitofp_p64_p16>(i.dst, i.src); }
			instr_case(high::Op_uitofp_p64_p16, i) { addLow<Op_uitofp_p64_p16>(i.dst, i.src); }
			instr_case(high::Op_sitofp_p64_p32, i) { addLow<Op_sitofp_p64_p32>(i.dst, i.src); }
			instr_case(high::Op_uitofp_p64_p32, i) { addLow<Op_uitofp_p64_p32>(i.dst, i.src); }
			instr_case(high::Op_sitofp_p64_p64, i) { addLow<Op_sitofp_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_uitofp_p64_p64, i) { addLow<Op_uitofp_p64_p64>(i.dst, i.src); }

			// Float to Int
			instr_case(high::Op_fptosi_p8_p32, i) { addLow<Op_fptosi_p8_p32>(i.dst, i.src); }
			instr_case(high::Op_fptoui_p8_p32, i) { addLow<Op_fptoui_p8_p32>(i.dst, i.src); }
			instr_case(high::Op_fptosi_p16_p32, i) { addLow<Op_fptosi_p16_p32>(i.dst, i.src); }
			instr_case(high::Op_fptoui_p16_p32, i) { addLow<Op_fptoui_p16_p32>(i.dst, i.src); }
			instr_case(high::Op_fptosi_p32_p32, i) { addLow<Op_fptosi_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_fptoui_p32_p32, i) { addLow<Op_fptoui_p32_p32>(i.dst, i.src); }
			instr_case(high::Op_fptosi_p64_p32, i) { addLow<Op_fptosi_p64_p32>(i.dst, i.src); }
			instr_case(high::Op_fptoui_p64_p32, i) { addLow<Op_fptoui_p64_p32>(i.dst, i.src); }
			instr_case(high::Op_fptosi_p8_p64, i) { addLow<Op_fptosi_p8_p64>(i.dst, i.src); }
			instr_case(high::Op_fptoui_p8_p64, i) { addLow<Op_fptoui_p8_p64>(i.dst, i.src); }
			instr_case(high::Op_fptosi_p16_p64, i) { addLow<Op_fptosi_p16_p64>(i.dst, i.src); }
			instr_case(high::Op_fptoui_p16_p64, i) { addLow<Op_fptoui_p16_p64>(i.dst, i.src); }
			instr_case(high::Op_fptosi_p32_p64, i) { addLow<Op_fptosi_p32_p64>(i.dst, i.src); }
			instr_case(high::Op_fptoui_p32_p64, i) { addLow<Op_fptoui_p32_p64>(i.dst, i.src); }
			instr_case(high::Op_fptosi_p64_p64, i) { addLow<Op_fptosi_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_fptoui_p64_p64, i) { addLow<Op_fptoui_p64_p64>(i.dst, i.src); }
			instr_case(high::Op_fptrunc_p32_p64, i) { addLow<Op_fptrunc_p32_p64>(i.dst, i.src); }
			instr_case(high::Op_fpext_p64_p32, i) { addLow<Op_fpext_p64_p32>(i.dst, i.src); }
			instr_case(high::Op_nop, i) { addLow<Op_nop>(); }
			instr_case(high::Op_exit, i) { addLow<Op_exit>(); }
			instr_case(high::Op_initFromVmValue, i) { addLow<Op_initFromVmValue>(); }
			instr_case(high::Comment, i) {
				// Do nothing
			}
		}
		POP_DIAGNOSTIC

		return { .begin = instruction_begin_index, .end = next_instruction_index };
	}
}
