#pragma once

#include "compiler.hpp"

#include <base/comptime/type_traits.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/thread/low_program/utils.hpp>

#include <tuple>
#include <type_traits>

namespace vm::loader::compiler::detail {
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
	class MicroBytecodeBuilder {
		Compiler&                             compiler;
		Compiler::FunctionCompilationContext& ctx;

		base::HashMap<usize, usize> label_id_to_offset{};
		usize                       next_instruction_index = 0;

		low::MicroBytecode result;

#if (BUILD_TYPE_DEV_DEBUG)
		std::string current_high_instruction_representation{};
#endif

	public:
		MicroBytecodeBuilder(Compiler& compiler, Compiler::FunctionCompilationContext& ctx):
			  compiler{ compiler },
			  ctx{ ctx } {}

		std::pair<low::MicroBytecode, decltype(label_id_to_offset)> build() {
			return { std::move(result), std::move(label_id_to_offset) };
		}

		/// Add a new high instruction.
		void add(const code::Instruction& instruction);


	private:
		template<typename LowArg, typename HighArg>
		requires IsTranslatableInstructionArgumentPair<LowArg, HighArg>
		u64 lowerLowArg(HighArg&& arg) {
			if constexpr (std::constructible_from<u64, HighArg>) {
				return u64(arg);
			} else {
				return compiler.template lowerArgument<std::remove_cvref_t<HighArg>, LowArg>(
					ctx, arg
				);
			}
		}

		template<typename T, typename... Args>
		requires AreTranslatableInstructionTagArgs<T, Args...> void addLow(Args&&... args) {
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
			usize lid = compiler.lowerArgument<opargs::Label, low::opargs::Label>(ctx, label);
			label_id_to_offset.put(lid, next_instruction_index);
		}
	};

	void MicroBytecodeBuilder::add(const code::Instruction& instruction) {
#if (BUILD_TYPE_DEV_DEBUG)
		current_high_instruction_representation = code::instructionToString(instruction);
#endif
		PUSH_DIAGNOSTIC
		UNHANDLED_ENUM
		instr_match(instruction) {
			instr_case(high::Op_mov_l8_imm, i) { addLow<Op_mov_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_l8_l8, i) { addLow<Op_mov_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_cmov_l8_l8, i) { addLow<Op_cmov_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_cmov_l8_imm, i) { addLow<Op_cmov_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_l16_imm, i) { addLow<Op_mov_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_l16_l16, i) { addLow<Op_mov_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_cmov_l16_l16, i) { addLow<Op_cmov_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_cmov_l16_imm, i) { addLow<Op_cmov_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_l32_imm, i) { addLow<Op_mov_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_l32_l32, i) { addLow<Op_mov_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_cmov_l32_l32, i) { addLow<Op_cmov_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_cmov_l32_imm, i) { addLow<Op_cmov_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_l64_imm, i) { addLow<Op_mov_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_l64_l64, i) { addLow<Op_mov_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_cmov_l64_l64, i) { addLow<Op_cmov_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_cmov_l64_imm, i) { addLow<Op_cmov_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_g64_g64, i) { addLow<Op_mov_g64_g64>(i.dst, i.src); }
			instr_case(high::Op_mov_g64_l64, i) { addLow<Op_mov_g64_l64>(i.dst, i.src); }
			instr_case(high::Op_mov_g64_imm, i) { addLow<Op_mov_g64_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_g32_g32, i) { addLow<Op_mov_g32_g32>(i.dst, i.src); }
			instr_case(high::Op_mov_g32_l32, i) { addLow<Op_mov_g32_l32>(i.dst, i.src); }
			instr_case(high::Op_mov_g32_imm, i) { addLow<Op_mov_g32_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_g16_g16, i) { addLow<Op_mov_g16_g16>(i.dst, i.src); }
			instr_case(high::Op_mov_g16_l16, i) { addLow<Op_mov_g16_l16>(i.dst, i.src); }
			instr_case(high::Op_mov_g16_imm, i) { addLow<Op_mov_g16_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_g8_g8, i) { addLow<Op_mov_g8_g8>(i.dst, i.src); }
			instr_case(high::Op_mov_g8_l8, i) { addLow<Op_mov_g8_l8>(i.dst, i.src); }
			instr_case(high::Op_mov_g8_imm, i) { addLow<Op_mov_g8_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_gptr_lptr, i) { addLow<Op_mov_gptr_lptr>(i.dst, i.src); }
			instr_case(high::Op_mov_l64_g64, i) { addLow<Op_mov_l64_g64>(i.dst, i.src); }
			instr_case(high::Op_mov_l32_g32, i) { addLow<Op_mov_l32_g32>(i.dst, i.src); }
			instr_case(high::Op_mov_l16_g16, i) { addLow<Op_mov_l16_g16>(i.dst, i.src); }
			instr_case(high::Op_mov_l8_g8, i) { addLow<Op_mov_l8_g8>(i.dst, i.src); }
			instr_case(high::Op_mov_lptr_gptr, i) { addLow<Op_mov_lptr_gptr>(i.dst, i.src); }
			instr_case(high::Op_mov_lptr_lptr, i) { addLow<Op_mov_lptr_lptr>(i.dst, i.src); }
			instr_case(high::Op_setNull_lptr, i) { addLow<Op_setNull_lptr>(i.dst); }
			instr_case(high::Op_mov_lopq_lopq, i) { addLow<Op_mov_blopq_blopq>(i.dst, i.src); }
			instr_case(high::Op_mov_lopq_gopq, i) { addLow<Op_mov_blopq_gopq>(i.dst, i.src); }
			instr_case(high::Op_mov_lopq_imm, i) { addLow<Op_mov_blopq_imm>(i.dst, i.src); }
			instr_case(high::Op_mov_gopq_lopq, i) { addLow<Op_mov_gopq_blopq>(i.dst, i.src); }
			instr_case(high::Op_mov_lste_lste, i) { addLow<Op_mov_blste_blste>(i.dst, i.src); }
			instr_case(high::Op_mov_lste_gste, i) { addLow<Op_mov_blste_gste>(i.dst, i.src); }
			instr_case(high::Op_mov_gste_lste, i) { addLow<Op_mov_gste_blste>(i.dst, i.src); }
			instr_case(high::Op_mov_gste_gste, i) { addLow<Op_mov_gste_gste>(i.dst, i.src); }
			instr_case(high::Op_add_l64_l64, i) { addLow<Op_add_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_add_l64_imm, i) { addLow<Op_add_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_add_l32_l32, i) { addLow<Op_add_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_add_l32_imm, i) { addLow<Op_add_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_add_l16_l16, i) { addLow<Op_add_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_add_l16_imm, i) { addLow<Op_add_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_add_l8_l8, i) { addLow<Op_add_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_add_l8_imm, i) { addLow<Op_add_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_sub_l64_l64, i) { addLow<Op_sub_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_sub_l64_imm, i) { addLow<Op_sub_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_sub_l32_l32, i) { addLow<Op_sub_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_sub_l32_imm, i) { addLow<Op_sub_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_sub_l16_l16, i) { addLow<Op_sub_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_sub_l16_imm, i) { addLow<Op_sub_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_sub_l8_l8, i) { addLow<Op_sub_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_sub_l8_imm, i) { addLow<Op_sub_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_mul_l64_l64, i) { addLow<Op_mul_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_mul_l64_imm, i) { addLow<Op_mul_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_mul_l32_l32, i) { addLow<Op_mul_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_mul_l32_imm, i) { addLow<Op_mul_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_mul_l16_l16, i) { addLow<Op_mul_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_mul_l16_imm, i) { addLow<Op_mul_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_mul_l8_l8, i) { addLow<Op_mul_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_mul_l8_imm, i) { addLow<Op_mul_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_div_l64_l64, i) { addLow<Op_div_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_div_l64_imm, i) { addLow<Op_div_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_div_l32_l32, i) { addLow<Op_div_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_div_l32_imm, i) { addLow<Op_div_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_div_l16_l16, i) { addLow<Op_div_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_div_l16_imm, i) { addLow<Op_div_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_div_l8_l8, i) { addLow<Op_div_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_div_l8_imm, i) { addLow<Op_div_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_mod_l64_l64, i) { addLow<Op_mod_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_mod_l64_imm, i) { addLow<Op_mod_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_mod_l32_l32, i) { addLow<Op_mod_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_mod_l32_imm, i) { addLow<Op_mod_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_mod_l16_l16, i) { addLow<Op_mod_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_mod_l16_imm, i) { addLow<Op_mod_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_mod_l8_l8, i) { addLow<Op_mod_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_mod_l8_imm, i) { addLow<Op_mod_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_neg_l64, i) { addLow<Op_neg_l64>(i.dst); }
			instr_case(high::Op_neg_l32, i) { addLow<Op_neg_l32>(i.dst); }
			instr_case(high::Op_neg_l16, i) { addLow<Op_neg_l16>(i.dst); }
			instr_case(high::Op_neg_l8, i) { addLow<Op_neg_l8>(i.dst); }
			instr_case(high::Op_fadd_l64_l64, i) { addLow<Op_fadd_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_fadd_l64_imm, i) { addLow<Op_fadd_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_fadd_l32_l32, i) { addLow<Op_fadd_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_fadd_l32_imm, i) { addLow<Op_fadd_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_fsub_l64_l64, i) { addLow<Op_fsub_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_fsub_l64_imm, i) { addLow<Op_fsub_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_fsub_l32_l32, i) { addLow<Op_fsub_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_fsub_l32_imm, i) { addLow<Op_fsub_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_fmul_l64_l64, i) { addLow<Op_fmul_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_fmul_l64_imm, i) { addLow<Op_fmul_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_fmul_l32_l32, i) { addLow<Op_fmul_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_fmul_l32_imm, i) { addLow<Op_fmul_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_fdiv_l64_l64, i) { addLow<Op_fdiv_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_fdiv_l64_imm, i) { addLow<Op_fdiv_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_fdiv_l32_l32, i) { addLow<Op_fdiv_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_fdiv_l32_imm, i) { addLow<Op_fdiv_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_fneg_l64, i) { addLow<Op_fneg_l64>(i.dst); }
			instr_case(high::Op_fneg_l32, i) { addLow<Op_fneg_l32>(i.dst); }
			instr_case(high::Op_umul_l64_l64, i) { addLow<Op_umul_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_umul_l64_imm, i) { addLow<Op_umul_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_umul_l32_l32, i) { addLow<Op_umul_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_umul_l32_imm, i) { addLow<Op_umul_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_umul_l16_l16, i) { addLow<Op_umul_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_umul_l16_imm, i) { addLow<Op_umul_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_umul_l8_l8, i) { addLow<Op_umul_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_umul_l8_imm, i) { addLow<Op_umul_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_umod_l64_l64, i) { addLow<Op_umod_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_umod_l64_imm, i) { addLow<Op_umod_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_umod_l32_l32, i) { addLow<Op_umod_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_umod_l32_imm, i) { addLow<Op_umod_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_umod_l16_l16, i) { addLow<Op_umod_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_umod_l16_imm, i) { addLow<Op_umod_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_umod_l8_l8, i) { addLow<Op_umod_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_umod_l8_imm, i) { addLow<Op_umod_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_udiv_l64_l64, i) { addLow<Op_udiv_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_udiv_l64_imm, i) { addLow<Op_udiv_l64_imm>(i.dst, i.src); }
			instr_case(high::Op_udiv_l32_l32, i) { addLow<Op_udiv_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_udiv_l32_imm, i) { addLow<Op_udiv_l32_imm>(i.dst, i.src); }
			instr_case(high::Op_udiv_l16_l16, i) { addLow<Op_udiv_l16_l16>(i.dst, i.src); }
			instr_case(high::Op_udiv_l16_imm, i) { addLow<Op_udiv_l16_imm>(i.dst, i.src); }
			instr_case(high::Op_udiv_l8_l8, i) { addLow<Op_udiv_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_udiv_l8_imm, i) { addLow<Op_udiv_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_log_and_l8_l8, i) { addLow<Op_log_and_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_log_and_l8_imm, i) { addLow<Op_log_and_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_log_or_l8_l8, i) { addLow<Op_log_or_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_log_or_l8_imm, i) { addLow<Op_log_or_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_log_xor_l8_l8, i) { addLow<Op_log_xor_l8_l8>(i.dst, i.src); }
			instr_case(high::Op_log_xor_l8_imm, i) { addLow<Op_log_xor_l8_imm>(i.dst, i.src); }
			instr_case(high::Op_log_not_l8, i) { addLow<Op_log_not_l8>(i.dst); }
			instr_case(high::Op_cmpEq_l64_l64, i) { addLow<Op_cmpEq_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_l64_imm, i) { addLow<Op_cmpEq_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_l64_l64, i) { addLow<Op_cmpNeq_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_l64_imm, i) { addLow<Op_cmpNeq_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_l64_l64, i) { addLow<Op_cmpGt_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_l64_imm, i) { addLow<Op_cmpGt_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_l64_l64, i) { addLow<Op_cmpGe_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_l64_imm, i) { addLow<Op_cmpGe_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_l64_l64, i) { addLow<Op_ucmpGt_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_l64_imm, i) { addLow<Op_ucmpGt_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_l64_l64, i) { addLow<Op_ucmpGe_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_l64_imm, i) { addLow<Op_ucmpGe_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_l64_l64, i) { addLow<Op_cmpLt_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_l64_imm, i) { addLow<Op_cmpLt_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_l64_l64, i) { addLow<Op_cmpLe_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_l64_imm, i) { addLow<Op_cmpLe_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_l64_l64, i) { addLow<Op_ucmpLt_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_l64_imm, i) { addLow<Op_ucmpLt_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_l64_l64, i) { addLow<Op_ucmpLe_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_l64_imm, i) { addLow<Op_ucmpLe_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_l32_l32, i) { addLow<Op_cmpEq_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_l32_imm, i) { addLow<Op_cmpEq_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_l32_l32, i) { addLow<Op_cmpNeq_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_l32_imm, i) { addLow<Op_cmpNeq_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_l32_l32, i) { addLow<Op_cmpGt_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_l32_imm, i) { addLow<Op_cmpGt_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_l32_l32, i) { addLow<Op_cmpGe_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_l32_imm, i) { addLow<Op_cmpGe_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_l32_l32, i) { addLow<Op_ucmpGt_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_l32_imm, i) { addLow<Op_ucmpGt_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_l32_l32, i) { addLow<Op_ucmpGe_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_l32_imm, i) { addLow<Op_ucmpGe_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_l32_l32, i) { addLow<Op_cmpLt_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_l32_imm, i) { addLow<Op_cmpLt_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_l32_l32, i) { addLow<Op_cmpLe_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_l32_imm, i) { addLow<Op_cmpLe_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_l32_l32, i) { addLow<Op_ucmpLt_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_l32_imm, i) { addLow<Op_ucmpLt_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_l32_l32, i) { addLow<Op_ucmpLe_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_l32_imm, i) { addLow<Op_ucmpLe_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_l16_l16, i) { addLow<Op_cmpEq_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_l16_imm, i) { addLow<Op_cmpEq_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_l16_l16, i) { addLow<Op_cmpNeq_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_l16_imm, i) { addLow<Op_cmpNeq_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_l16_l16, i) { addLow<Op_cmpGt_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_l16_imm, i) { addLow<Op_cmpGt_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_l16_l16, i) { addLow<Op_cmpGe_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_l16_imm, i) { addLow<Op_cmpGe_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_l16_l16, i) { addLow<Op_ucmpGt_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_l16_imm, i) { addLow<Op_ucmpGt_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_l16_l16, i) { addLow<Op_ucmpGe_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_l16_imm, i) { addLow<Op_ucmpGe_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_l16_l16, i) { addLow<Op_cmpLt_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_l16_imm, i) { addLow<Op_cmpLt_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_l16_l16, i) { addLow<Op_cmpLe_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_l16_imm, i) { addLow<Op_cmpLe_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_l16_l16, i) { addLow<Op_ucmpLt_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_l16_imm, i) { addLow<Op_ucmpLt_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_l16_l16, i) { addLow<Op_ucmpLe_l16_l16>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_l16_imm, i) { addLow<Op_ucmpLe_l16_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_l8_l8, i) { addLow<Op_cmpEq_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpEq_l8_imm, i) { addLow<Op_cmpEq_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_l8_l8, i) { addLow<Op_cmpNeq_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNeq_l8_imm, i) { addLow<Op_cmpNeq_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_l8_l8, i) { addLow<Op_cmpGt_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGt_l8_imm, i) { addLow<Op_cmpGt_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_l8_l8, i) { addLow<Op_cmpGe_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpGe_l8_imm, i) { addLow<Op_cmpGe_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_l8_l8, i) { addLow<Op_ucmpGt_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGt_l8_imm, i) { addLow<Op_ucmpGt_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_l8_l8, i) { addLow<Op_ucmpGe_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpGe_l8_imm, i) { addLow<Op_ucmpGe_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_l8_l8, i) { addLow<Op_cmpLt_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLt_l8_imm, i) { addLow<Op_cmpLt_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_l8_l8, i) { addLow<Op_cmpLe_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpLe_l8_imm, i) { addLow<Op_cmpLe_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_l8_l8, i) { addLow<Op_ucmpLt_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLt_l8_imm, i) { addLow<Op_ucmpLt_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_l8_l8, i) { addLow<Op_ucmpLe_l8_l8>(i.lhs, i.rhs); }
			instr_case(high::Op_ucmpLe_l8_imm, i) { addLow<Op_ucmpLe_l8_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpEq_l64_l64, i) { addLow<Op_fcmpEq_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpEq_l64_imm, i) { addLow<Op_fcmpEq_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpNeq_l64_l64, i) { addLow<Op_fcmpNeq_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpNeq_l64_imm, i) { addLow<Op_fcmpNeq_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGt_l64_l64, i) { addLow<Op_fcmpGt_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGt_l64_imm, i) { addLow<Op_fcmpGt_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGe_l64_l64, i) { addLow<Op_fcmpGe_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGe_l64_imm, i) { addLow<Op_fcmpGe_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLt_l64_l64, i) { addLow<Op_fcmpLt_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLt_l64_imm, i) { addLow<Op_fcmpLt_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLe_l64_l64, i) { addLow<Op_fcmpLe_l64_l64>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLe_l64_imm, i) { addLow<Op_fcmpLe_l64_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpEq_l32_l32, i) { addLow<Op_fcmpEq_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpEq_l32_imm, i) { addLow<Op_fcmpEq_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpNeq_l32_l32, i) { addLow<Op_fcmpNeq_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpNeq_l32_imm, i) { addLow<Op_fcmpNeq_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGt_l32_l32, i) { addLow<Op_fcmpGt_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGt_l32_imm, i) { addLow<Op_fcmpGt_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGe_l32_l32, i) { addLow<Op_fcmpGe_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpGe_l32_imm, i) { addLow<Op_fcmpGe_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLt_l32_l32, i) { addLow<Op_fcmpLt_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLt_l32_imm, i) { addLow<Op_fcmpLt_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLe_l32_l32, i) { addLow<Op_fcmpLe_l32_l32>(i.lhs, i.rhs); }
			instr_case(high::Op_fcmpLe_l32_imm, i) { addLow<Op_fcmpLe_l32_imm>(i.lhs, i.rhs); }
			instr_case(high::Op_cmpNull_lptr, i) { addLow<Op_cmpNull_lptr>(i.ptr); }
			instr_case(high::Op_variantSetInner_lvnt_type, i) {
				addLow<Op_variantSetInner_blvnt_type>(i.variant, i.inner_type);
				opargs::Type variant_type = ctx.locals_map.at(i.variant.var_name).type->getName();
				addLow<Op_ext_type>(variant_type);
			}
			instr_case(high::Op_variantGetInner_lptr_lvnt_type, i) {
				addLow<Op_variantGetInner_lptr_blvnt>(i.dst_ptr, i.variant);
				opargs::Type variant_type = ctx.locals_map.at(i.variant.var_name).type->getName();
				addLow<Op_ext_type_type>(i.expected_type, variant_type);
			}
			instr_case(high::Op_variantSetInner_lptr_type, i) {
				addLow<Op_variantSetInner_lptr_type>(i.variant_ptr, i.inner_type);
				opargs::Type variant_type = ctx.locals_map.at(i.variant_ptr.var_name)
				                                .type->getInnerType()
				                                .value()
				                                ->getName();
				addLow<Op_ext_type>(variant_type);
			}
			instr_case(high::Op_variantGetInner_lptr_lptr_type, i) {
				addLow<Op_variantGetInner_lptr_lptr>(i.dst_ptr, i.variant_ptr);
				opargs::Type variant_type = ctx.locals_map.at(i.variant_ptr.var_name)
				                                .type->getInnerType()
				                                .value()
				                                ->getName();
				addLow<Op_ext_type_type>(i.expected_type, variant_type);
			}
			instr_case(high::Op_label, i) { addLabel(i.label); }
			instr_case(high::Op_jmp_label, i) { addLow<Op_jmp_label>(i.label); }
			instr_case(high::Op_jmpIf_label, i) { addLow<Op_jmpIf_label>(i.label); }
			instr_case(high::Op_jmpIfNot_label, i) { addLow<Op_jmpIfNot_label>(i.label); }
			instr_case(high::Op_call_func, i) {
#ifdef ENABLE_JIT
				addLow<Op_jit_call_entrypoint>(i.function);
#else
				addLow<Op_call_func>(i.function);
#endif
			}
			instr_case(high::Op_call_builtinfunc, i) { addLow<Op_call_builtinfunc>(i.function); }
			instr_case(high::Op_call_cfunc, i) { addLow<Op_call_cfunc>(i.function); }
			instr_case(high::Op_set_threadctx, i) { addLow<Op_set_threadctx>(i.function); }
			instr_case(high::Op_ret_tailcall_func, i) { addLow<Op_ret_tailcall_func>(i.function); }
			instr_case(high::Op_ret, i) { addLow<Op_ret>(); }
			instr_case(high::Op_init_lany_type, i) { addLow<Op_init_blany_type>(i.var, i.type); }
			instr_case(high::Op_deinit, i) { addLow<Op_deinit>(); }
			instr_case(high::Op_input_l64, i) { addLow<Op_input_l64>(i.dst); }
			instr_case(high::Op_output_l64, i) { addLow<Op_output_l64>(i.src); }
			instr_case(high::Op_input_l32, i) { addLow<Op_input_l32>(i.dst); }
			instr_case(high::Op_output_l32, i) { addLow<Op_output_l32>(i.src); }
			instr_case(high::Op_setVTable_lptr_type, i) {
				addLow<Op_setVTable_lptr_type>(i.object_ptr, i.type);
			}
			instr_case(high::Op_resetVTable_lptr, i) { addLow<Op_resetVTable_lptr>(i.object_ptr); }
			instr_case(high::Op_upcast_lptr_lptr, i) { addLow<Op_upcast_lptr_lptr>(i.dst, i.src); }
			instr_case(high::Op_downcast_lptr_lptr, i) {
				addLow<Op_downcast_lptr_lptr>(i.dst, i.src);
				opargs::Type variant_type
					= ctx.locals_map.at(i.dst.var_name).type->getInnerType().value()->getName();
				addLow<Op_ext_type>(variant_type);
			}
			instr_case(high::Op_virtual_call_lptr_method, i) {
				addLow<Op_virtual_call_lptr_method>(i.object_ptr, i.method);
			}
			instr_case(high::Op_alloc_lptr_type, i) { addLow<Op_alloc_lptr_type>(i.ptr, i.type); }
			instr_case(high::Op_free_lptr, i) { addLow<Op_free_lptr>(i.ptr); }
			instr_case(high::Op_store_lptr_lany, i) {
				addLow<Op_store_lptr_blany>(i.dst_ptr, i.src);
			}
			instr_case(high::Op_load_lany_lptr, i) { addLow<Op_load_blany_lptr>(i.dst, i.src_ptr); }
			instr_case(high::Op_ref_lptr_lany, i) { addLow<Op_ref_lptr_blany>(i.dst_ptr, i.src); }
			instr_case(high::Op_ref_lptr_gany, i) { addLow<Op_ref_lptr_gany>(i.dst_ptr, i.src); }
			instr_case(high::Op_structLea_lptr_lptr_field, i) {
				addLow<Op_structLea_lptr_lptr>(i.dst_ptr, i.src_data_ptr);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structLoad_lany_lptr_field, i) {
				addLow<Op_structLoad_blany_lptr>(i.dst, i.src_data_ptr);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structStore_lptr_lany_field, i) {
				addLow<Op_structStore_lptr_blany>(i.dst_data_ptr, i.src);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structLea_lptr_lste_field, i) {
				addLow<Op_structLea_lptr_blste>(i.dst_ptr, i.src_data_struct);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structLoad_lany_lste_field, i) {
				addLow<Op_structLoad_blany_blste>(i.dst, i.src_data_struct);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_structStore_lste_lany_field, i) {
				addLow<Op_structStore_blste_blany>(i.dst_data_struct, i.src);
				addLow<Op_ext_field>(i.field);
			}
			instr_case(high::Op_fixedSizeTableLea_lptr_lptr_l64, i) {
				addLow<Op_fixedSizeTableLea_lptr_lptr>(i.dst_ptr, i.src_table_ptr);
				addLow<Op_ext_l64>(i.index);
			}
			instr_case(high::Op_fixedSizeTableLoad_lany_lptr_l64, i) {
				addLow<Op_fixedSizeTableLoad_blany_lptr>(i.dst, i.src_table_ptr);
				addLow<Op_ext_l64>(i.index);
			}
			instr_case(high::Op_fixedSizeTableStore_lptr_lany_l64, i) {
				addLow<Op_fixedSizeTableStore_lptr_blany>(i.dst_table_ptr, i.src);
				addLow<Op_ext_l64>(i.index);
			}
			instr_case(high::Op_dynTableLea_lptr_lptr_l64, i) {
				addLow<Op_dynTableLea_lptr_lptr>(i.dst_ptr, i.src_table_ptr);
				addLow<Op_ext_l64>(i.index);
			}
			instr_case(high::Op_dynTableLoad_lany_lptr_l64, i) {
				addLow<Op_dynTableLoad_blany_lptr>(i.dst, i.src_table_ptr);
				addLow<Op_ext_l64>(i.index);
			}
			instr_case(high::Op_dynTableStore_lptr_lany_l64, i) {
				addLow<Op_dynTableStore_lptr_blany>(i.dst_table_ptr, i.src);
				addLow<Op_ext_l64>(i.index);
			}
			instr_case(high::Op_dynTableReAlloc_lptr_type_l64, i) {
				addLow<Op_dynTableReAlloc_lptr_type>(i.dst_table_ptr, i.table_type);
				addLow<Op_ext_l64>(i.new_elem_count);
			}
			instr_case(high::Op_strOutput_lptr, i) { addLow<Op_strOutput_lptr>(i.string_ptr); }
			instr_case(high::Op_cast_l8_type, i) {
				addLow<Op_cast_l8_type>(i.value, i.target_type);
			}
			instr_case(high::Op_cast_l16_type, i) {
				addLow<Op_cast_l16_type>(i.value, i.target_type);
			}
			instr_case(high::Op_cast_l32_type, i) {
				addLow<Op_cast_l32_type>(i.value, i.target_type);
			}
			instr_case(high::Op_cast_l64_type, i) {
				addLow<Op_cast_l64_type>(i.value, i.target_type);
			}  // Sign Extension
			instr_case(high::Op_sext_l16_l8, i) { addLow<Op_sext_l16_l8>(i.dst, i.src); }
			instr_case(high::Op_sext_l32_l8, i) { addLow<Op_sext_l32_l8>(i.dst, i.src); }
			instr_case(high::Op_sext_l64_l8, i) { addLow<Op_sext_l64_l8>(i.dst, i.src); }
			instr_case(high::Op_sext_l32_l16, i) { addLow<Op_sext_l32_l16>(i.dst, i.src); }
			instr_case(high::Op_sext_l64_l16, i) { addLow<Op_sext_l64_l16>(i.dst, i.src); }
			instr_case(high::Op_sext_l64_l32, i) { addLow<Op_sext_l64_l32>(i.dst, i.src); }

			// Zero Extension
			instr_case(high::Op_zext_l16_l8, i) { addLow<Op_zext_l16_l8>(i.dst, i.src); }
			instr_case(high::Op_zext_l32_l8, i) { addLow<Op_zext_l32_l8>(i.dst, i.src); }
			instr_case(high::Op_zext_l64_l8, i) { addLow<Op_zext_l64_l8>(i.dst, i.src); }
			instr_case(high::Op_zext_l32_l16, i) { addLow<Op_zext_l32_l16>(i.dst, i.src); }
			instr_case(high::Op_zext_l64_l16, i) { addLow<Op_zext_l64_l16>(i.dst, i.src); }
			instr_case(high::Op_zext_l64_l32, i) { addLow<Op_zext_l64_l32>(i.dst, i.src); }

			// Truncation
			instr_case(high::Op_trunc_l8_l16, i) { addLow<Op_trunc_l8_l16>(i.dst, i.src); }
			instr_case(high::Op_trunc_l8_l32, i) { addLow<Op_trunc_l8_l32>(i.dst, i.src); }
			instr_case(high::Op_trunc_l8_l64, i) { addLow<Op_trunc_l8_l64>(i.dst, i.src); }
			instr_case(high::Op_trunc_l16_l32, i) { addLow<Op_trunc_l16_l32>(i.dst, i.src); }
			instr_case(high::Op_trunc_l16_l64, i) { addLow<Op_trunc_l16_l64>(i.dst, i.src); }
			instr_case(high::Op_trunc_l32_l64, i) { addLow<Op_trunc_l32_l64>(i.dst, i.src); }

			// Int to Float
			instr_case(high::Op_sitofp_l32_l8, i) { addLow<Op_sitofp_l32_l8>(i.dst, i.src); }
			instr_case(high::Op_uitofp_l32_l8, i) { addLow<Op_uitofp_l32_l8>(i.dst, i.src); }
			instr_case(high::Op_sitofp_l32_l16, i) { addLow<Op_sitofp_l32_l16>(i.dst, i.src); }
			instr_case(high::Op_uitofp_l32_l16, i) { addLow<Op_uitofp_l32_l16>(i.dst, i.src); }
			instr_case(high::Op_sitofp_l32_l32, i) { addLow<Op_sitofp_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_uitofp_l32_l32, i) { addLow<Op_uitofp_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_sitofp_l32_l64, i) { addLow<Op_sitofp_l32_l64>(i.dst, i.src); }
			instr_case(high::Op_uitofp_l32_l64, i) { addLow<Op_uitofp_l32_l64>(i.dst, i.src); }

			instr_case(high::Op_sitofp_l64_l8, i) { addLow<Op_sitofp_l64_l8>(i.dst, i.src); }
			instr_case(high::Op_uitofp_l64_l8, i) { addLow<Op_uitofp_l64_l8>(i.dst, i.src); }
			instr_case(high::Op_sitofp_l64_l16, i) { addLow<Op_sitofp_l64_l16>(i.dst, i.src); }
			instr_case(high::Op_uitofp_l64_l16, i) { addLow<Op_uitofp_l64_l16>(i.dst, i.src); }
			instr_case(high::Op_sitofp_l64_l32, i) { addLow<Op_sitofp_l64_l32>(i.dst, i.src); }
			instr_case(high::Op_uitofp_l64_l32, i) { addLow<Op_uitofp_l64_l32>(i.dst, i.src); }
			instr_case(high::Op_sitofp_l64_l64, i) { addLow<Op_sitofp_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_uitofp_l64_l64, i) { addLow<Op_uitofp_l64_l64>(i.dst, i.src); }

			// Float to Int
			instr_case(high::Op_fptosi_l8_l32, i) { addLow<Op_fptosi_l8_l32>(i.dst, i.src); }
			instr_case(high::Op_fptoui_l8_l32, i) { addLow<Op_fptoui_l8_l32>(i.dst, i.src); }
			instr_case(high::Op_fptosi_l16_l32, i) { addLow<Op_fptosi_l16_l32>(i.dst, i.src); }
			instr_case(high::Op_fptoui_l16_l32, i) { addLow<Op_fptoui_l16_l32>(i.dst, i.src); }
			instr_case(high::Op_fptosi_l32_l32, i) { addLow<Op_fptosi_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_fptoui_l32_l32, i) { addLow<Op_fptoui_l32_l32>(i.dst, i.src); }
			instr_case(high::Op_fptosi_l64_l32, i) { addLow<Op_fptosi_l64_l32>(i.dst, i.src); }
			instr_case(high::Op_fptoui_l64_l32, i) { addLow<Op_fptoui_l64_l32>(i.dst, i.src); }

			instr_case(high::Op_fptosi_l8_l64, i) { addLow<Op_fptosi_l8_l64>(i.dst, i.src); }
			instr_case(high::Op_fptoui_l8_l64, i) { addLow<Op_fptoui_l8_l64>(i.dst, i.src); }
			instr_case(high::Op_fptosi_l16_l64, i) { addLow<Op_fptosi_l16_l64>(i.dst, i.src); }
			instr_case(high::Op_fptoui_l16_l64, i) { addLow<Op_fptoui_l16_l64>(i.dst, i.src); }
			instr_case(high::Op_fptosi_l32_l64, i) { addLow<Op_fptosi_l32_l64>(i.dst, i.src); }
			instr_case(high::Op_fptoui_l32_l64, i) { addLow<Op_fptoui_l32_l64>(i.dst, i.src); }
			instr_case(high::Op_fptosi_l64_l64, i) { addLow<Op_fptosi_l64_l64>(i.dst, i.src); }
			instr_case(high::Op_fptoui_l64_l64, i) { addLow<Op_fptoui_l64_l64>(i.dst, i.src); }

			instr_case(high::Op_fptrunc_l32_l64, i) { addLow<Op_fptrunc_l32_l64>(i.dst, i.src); }
			instr_case(high::Op_fpext_l64_l32, i) { addLow<Op_fpext_l64_l32>(i.dst, i.src); }

			instr_case(high::Op_nop, i) { addLow<Op_nop>(); }
			instr_case(high::Op_exit, i) { addLow<Op_exit>(); }
			instr_case(high::Op_breakpoint, i) { addLow<Op_breakpoint>(); }
			instr_case(high::Op_initFromVmValue, i) { addLow<Op_initFromVmValue>(); }
			instr_case(high::Comment, i) {
				// Do nothing
			}
		}
		POP_DIAGNOSTIC
	}
}
