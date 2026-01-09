#pragma once

#include "compiler.hpp"

#include <base/preproc/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/thread/low_program/utils.hpp>

namespace vm::loader::compiler::detail {

	namespace high = vm::code::instructions;
	using namespace vm::low::instruction_tags;

	/**
	 * Helper class for lowering high bytecode instructions.
	 * This class simply holds all the relevant context and defines some helper methods, which make
	 * defining instruction lowering recipes free from extra context arguments noise, typesafe and
	 * macro-free. When adding a new instruction simply provide a new `lower` specialisation like so:
	 * ```
	 * template<>
	 * void MicroBytecodeBuilder::lower<high::Op_do_something_complex>(
	 *     opargs::Foo foo, opargs::Bar bar
	 * ) {
	 *     addLow<Op_first_step>(foo, bar);
	 *     addLow<Op_second_step>(foo);
	 *     addLow<Op_finish_up_the_thing>();
	 * }
	 * ```
	 * You will get a compile time error (sadly a big one) if you forget to implement lowering for
	 * an instruction. Micro instruction arguments are type-checked.
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
		void add(const code::Instruction instruction);


	private:
		// Must be specialized per high-level instruction. Intentionally `=delete`d so a missing
		// specialization produces a clear compile-time error (early, in editor, not at linking).
		// Keep NOLINT because clang-tidy likes to have all `=delete` public. The rule is made for
		// enforcing `Foo() = delete` over private constructors, but here the specialisations
		// get "un-deleted" and this method is not meant to be called by the outside world.
		template<code::IsInstruction T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes>
		void lower(Args...) = delete;  // NOLINT(modernize-use-equals-delete)

		template<IsMicroInstructionTag T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void addLow(Args... args) {
			result.push_back(makeLowInstruction(T::OPCODE, compiler.lowerArgument(ctx, args)...));
#if (BUILD_TYPE_DEV_DEBUG)
			result.back().opcode_id      = T::OPCODE;
			result.back().representation = current_high_instruction_representation;
#endif
			next_instruction_index++;
		}

		void addLabel(opargs::Label label) {
			usize lid = compiler.lowerArgument(ctx, label);
			label_id_to_offset.put(lid, next_instruction_index);
		}
	};

	// Lowering recipes:
	// -----------------

	template<>
	void MicroBytecodeBuilder::lower<high::Comment>() {
		// emit nothing
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_label>(opargs::Label label) {
		addLabel(label);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mov_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_mov_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmov_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_cmov_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmov_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmov_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l16_imm>(
		vm::opargs::StackLocal16 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mov_l16_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l16_l16>(
		vm::opargs::StackLocal16 arg0, vm::opargs::StackLocal16 arg1
	) {
		addLow<Op_mov_l16_l16>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmov_l16_l16>(
		vm::opargs::StackLocal16 arg0, vm::opargs::StackLocal16 arg1
	) {
		addLow<Op_cmov_l16_l16>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmov_l16_imm>(
		vm::opargs::StackLocal16 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmov_l16_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mov_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_mov_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmov_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_cmov_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmov_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmov_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mov_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_mov_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmov_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_cmov_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmov_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmov_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g64_g64>(
		vm::opargs::Global64 arg0, vm::opargs::Global64 arg1
	) {
		addLow<Op_mov_g64_g64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g64_l64>(
		vm::opargs::Global64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_mov_g64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g64_imm>(
		vm::opargs::Global64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mov_g64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g32_g32>(
		vm::opargs::Global32 arg0, vm::opargs::Global32 arg1
	) {
		addLow<Op_mov_g32_g32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g32_l32>(
		vm::opargs::Global32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_mov_g32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g32_imm>(
		vm::opargs::Global32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mov_g32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g16_g16>(
		vm::opargs::Global16 arg0, vm::opargs::Global16 arg1
	) {
		addLow<Op_mov_g16_g16>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g16_l16>(
		vm::opargs::Global16 arg0, vm::opargs::StackLocal16 arg1
	) {
		addLow<Op_mov_g16_l16>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g16_imm>(
		vm::opargs::Global16 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mov_g16_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g8_g8>(
		vm::opargs::Global8 arg0, vm::opargs::Global8 arg1
	) {
		addLow<Op_mov_g8_g8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g8_l8>(
		vm::opargs::Global8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_mov_g8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_g8_imm>(
		vm::opargs::Global8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mov_g8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_gptr_lptr>(
		vm::opargs::GlobalPtr arg0, vm::opargs::StackLocalPtr arg1
	) {
		addLow<Op_mov_gptr_lptr>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l64_g64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Global64 arg1
	) {
		addLow<Op_mov_l64_g64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l32_g32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Global32 arg1
	) {
		addLow<Op_mov_l32_g32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l16_g16>(
		vm::opargs::StackLocal16 arg0, vm::opargs::Global16 arg1
	) {
		addLow<Op_mov_l16_g16>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_l8_g8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Global8 arg1
	) {
		addLow<Op_mov_l8_g8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_lptr_gptr>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::GlobalPtr arg1
	) {
		addLow<Op_mov_lptr_gptr>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_lptr_lptr>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::StackLocalPtr arg1
	) {
		addLow<Op_mov_lptr_lptr>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_setNull_lptr>(vm::opargs::StackLocalPtr arg0) {
		addLow<Op_setNull_lptr>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mov_lopq_lopq>(
		vm::opargs::StackLocalOpq arg0, vm::opargs::StackLocalOpq arg1
	) {
		addLow<Op_mov_lopq_lopq>(arg0, arg1);
	}

	// ========= ARITHMETIC OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_add_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_add_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_add_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_add_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_add_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_add_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_add_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_add_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_sub_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_sub_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_sub_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_sub_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_sub_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_sub_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_sub_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_sub_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mul_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_mul_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mul_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mul_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mul_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_mul_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mul_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mul_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mod_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_mod_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mod_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mod_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mod_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_mod_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_mod_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_mod_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_div_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_div_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_div_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_div_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_div_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_div_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_div_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_div_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_neg_l64>(vm::opargs::StackLocal64 arg0) {
		addLow<Op_neg_l64>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_neg_l32>(vm::opargs::StackLocal32 arg0) {
		addLow<Op_neg_l32>(arg0);
	}

	// ========= FLOATING POINT OPERATIONS ========
	template<>
	void MicroBytecodeBuilder::lower<high::Op_fadd_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_fadd_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fadd_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_fadd_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fadd_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_fadd_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fadd_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_fadd_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fsub_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_fsub_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fsub_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_fsub_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fsub_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_fsub_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fsub_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_fsub_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fmul_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_fmul_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fmul_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_fmul_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fmul_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_fmul_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fmul_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_fmul_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fdiv_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_fdiv_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fdiv_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_fdiv_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fdiv_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_fdiv_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fdiv_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_fdiv_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fneg_l64>(vm::opargs::StackLocal64 arg0) {
		addLow<Op_fneg_l64>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fneg_l32>(vm::opargs::StackLocal32 arg0) {
		addLow<Op_fneg_l32>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_umul_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_umul_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_umul_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_umul_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_umul_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_umul_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_umul_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_umul_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_umod_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_umod_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_umod_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_umod_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_umod_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_umod_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_umod_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_umod_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_udiv_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_udiv_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_udiv_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_udiv_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_udiv_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_udiv_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_udiv_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_udiv_l32_imm>(arg0, arg1);
	}

	// ========= BOOLEAN OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_log_and_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_log_and_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_log_and_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_log_and_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_log_or_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_log_or_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_log_or_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_log_or_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_log_xor_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_log_xor_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_log_xor_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_log_xor_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_log_not_l8>(vm::opargs::StackLocal8 arg0) {
		addLow<Op_log_not_l8>(arg0);
	}

	// ========= LOGICAL OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpEq_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_cmpEq_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpEq_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpEq_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpG_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_cmpG_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpG_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpG_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpG_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_ucmpG_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpG_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_ucmpG_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpL_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_cmpL_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpL_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpL_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpL_l64_l64>(
		vm::opargs::StackLocal64 arg0, vm::opargs::StackLocal64 arg1
	) {
		addLow<Op_ucmpL_l64_l64>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpL_l64_imm>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_ucmpL_l64_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpEq_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_cmpEq_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpEq_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpEq_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpG_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_cmpG_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpG_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpG_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpG_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_ucmpG_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpG_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_ucmpG_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpL_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_cmpL_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpL_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpL_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpL_l32_l32>(
		vm::opargs::StackLocal32 arg0, vm::opargs::StackLocal32 arg1
	) {
		addLow<Op_ucmpL_l32_l32>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpL_l32_imm>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_ucmpL_l32_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpEq_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_cmpEq_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpEq_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpEq_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpG_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_cmpG_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpG_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpG_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpG_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_ucmpG_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpG_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_ucmpG_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpL_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_cmpL_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpL_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_cmpL_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpL_l8_l8>(
		vm::opargs::StackLocal8 arg0, vm::opargs::StackLocal8 arg1
	) {
		addLow<Op_ucmpL_l8_l8>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ucmpL_l8_imm>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Immediate arg1
	) {
		addLow<Op_ucmpL_l8_imm>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cmpNull_lptr>(vm::opargs::StackLocalPtr arg0) {
		addLow<Op_cmpNull_lptr>(arg0);
	}

	// ========= VARIANT OPERATIONS ========


	template<>
	void MicroBytecodeBuilder::lower<high::Op_variantSetInner_lvnt_type>(
		vm::opargs::StackLocalVnt arg0, vm::opargs::Type arg1
	) {
		addLow<Op_variantSetInner_lvnt_type>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_variantGetInner_lptr_lvnt_type>(
		vm::opargs::StackLocalPtr dst, vm::opargs::StackLocalVnt vnt, vm::opargs::Type expected_type
	) {
		addLow<Op_variantGetInner_lptr_lvnt>(dst, vnt);
		addLow<Op_ext_type>(expected_type);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_variantSetInner_lptr_type>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::Type arg1
	) {
		addLow<Op_variantSetInner_lptr_type>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_variantGetInner_lptr_lptr_type>(
		vm::opargs::StackLocalPtr dst,
		vm::opargs::StackLocalPtr vnt_ptr,
		vm::opargs::Type          expected_type
	) {
		addLow<Op_variantGetInner_lptr_lptr>(dst, vnt_ptr);
		addLow<Op_ext_type>(expected_type);
	}

	// ========= JUMPS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_jmp_label>(vm::opargs::Label arg0) {
		addLow<Op_jmp_label>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_jmpIf_label>(vm::opargs::Label arg0) {
		addLow<Op_jmpIf_label>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_jmpIfNot_label>(vm::opargs::Label arg0) {
		addLow<Op_jmpIfNot_label>(arg0);
	}

	// ========= FUNCTION OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_call_func>(vm::opargs::FunctionName arg0) {
		addLow<Op_call_func>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_call_builtinfunc>(vm::opargs::BuiltinFunctionName arg0
	) {
		addLow<Op_call_builtinfunc>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_call_cfunc>(vm::opargs::ExtCFunctionName arg0) {
		addLow<Op_call_cfunc>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ret_tailcall_func>(vm::opargs::FunctionName arg0) {
		addLow<Op_ret_tailcall_func>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ret>() {
		addLow<Op_ret>();
	}

	// ========= STACK OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_init_lany_type>(
		vm::opargs::StackLocalAny arg0, vm::opargs::Type arg1
	) {
		addLow<Op_init_lany_type>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_deinit>() {
		addLow<Op_deinit>();
	}

	// ========= IO OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_input_l64>(vm::opargs::StackLocal64 arg0) {
		addLow<Op_input_l64>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_output_l64>(vm::opargs::StackLocal64 arg0) {
		addLow<Op_output_l64>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_input_l32>(vm::opargs::StackLocal32 arg0) {
		addLow<Op_input_l32>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_output_l32>(vm::opargs::StackLocal32 arg0) {
		addLow<Op_output_l32>(arg0);
	}

	// ========= CLASS OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_setVTable_lptr_type>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::Type arg1
	) {
		addLow<Op_setVTable_lptr_type>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_resetVTable_lptr>(
		vm::opargs::StackLocalPtr arg0
	) {
		addLow<Op_resetVTable_lptr>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_upcast_lptr_lptr>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::StackLocalPtr arg1
	) {
		addLow<Op_upcast_lptr_lptr>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_downcast_lptr_lptr_type>(
		vm::opargs::StackLocalPtr dst, vm::opargs::StackLocalPtr src, vm::opargs::Type type
	) {
		addLow<Op_downcast_lptr_lptr>(dst, src);
		addLow<Op_ext_type>(type);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_virtual_call_lptr_method>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::MethodName arg1
	) {
		addLow<Op_virtual_call_lptr_method>(arg0, arg1);
	}

	// ========= GENERAL POINTER OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_alloc_lptr_type>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::Type arg1
	) {
		addLow<Op_alloc_lptr_type>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_free_lptr>(vm::opargs::StackLocalPtr arg0) {
		addLow<Op_free_lptr>(arg0);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_store_lptr_lany>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::StackLocalAny arg1
	) {
		addLow<Op_store_lptr_lany>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_load_lany_lptr>(
		vm::opargs::StackLocalAny arg0, vm::opargs::StackLocalPtr arg1
	) {
		addLow<Op_load_lany_lptr>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_ref_lptr_lany>(
		vm::opargs::StackLocalPtr arg0, vm::opargs::StackLocalAny arg1
	) {
		addLow<Op_ref_lptr_lany>(arg0, arg1);
	}

	// ========= STRUCTURE OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_structLea_lptr_lptr_field>(
		vm::opargs::StackLocalPtr dst, vm::opargs::StackLocalPtr strukt, vm::opargs::Field field
	) {
		addLow<Op_structLea_lptr_lptr>(dst, strukt);
		addLow<Op_ext_field>(field);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_structLoad_lany_lptr_field>(
		vm::opargs::StackLocalAny dst, vm::opargs::StackLocalPtr strukt_ptr, vm::opargs::Field field
	) {
		addLow<Op_structLoad_lany_lptr>(dst, strukt_ptr);
		addLow<Op_ext_field>(field);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_structStore_lptr_lany_field>(
		vm::opargs::StackLocalPtr strukt_ptr, vm::opargs::StackLocalAny src, vm::opargs::Field field
	) {
		addLow<Op_structStore_lptr_lany>(strukt_ptr, src);
		addLow<Op_ext_field>(field);
	}

	// ========= TABLE OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fixedSizeTableLea_lptr_lptr_l64>(
		vm::opargs::StackLocalPtr dst,
		vm::opargs::StackLocalPtr table_ptr,
		vm::opargs::StackLocal64  index
	) {
		addLow<Op_fixedSizeTableLea_lptr_lptr>(dst, table_ptr);
		addLow<Op_ext_l64>(index);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fixedSizeTableLoad_lany_lptr_l64>(
		vm::opargs::StackLocalAny dst,
		vm::opargs::StackLocalPtr table_ptr,
		vm::opargs::StackLocal64  index
	) {
		addLow<Op_fixedSizeTableLoad_lany_lptr>(dst, table_ptr);
		addLow<Op_ext_l64>(index);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_fixedSizeTableStore_lptr_lany_l64>(
		vm::opargs::StackLocalPtr table_ptr,
		vm::opargs::StackLocalAny src,
		vm::opargs::StackLocal64  index
	) {
		addLow<Op_fixedSizeTableStore_lptr_lany>(table_ptr, src);
		addLow<Op_ext_l64>(index);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_dynTableLea_lptr_lptr_l64>(
		vm::opargs::StackLocalPtr dst,
		vm::opargs::StackLocalPtr table_ptr,
		vm::opargs::StackLocal64  index
	) {
		addLow<Op_dynTableLea_lptr_lptr>(dst, table_ptr);
		addLow<Op_ext_l64>(index);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_dynTableLoad_lany_lptr_l64>(
		vm::opargs::StackLocalAny dst,
		vm::opargs::StackLocalPtr table_ptr,
		vm::opargs::StackLocal64  index
	) {
		addLow<Op_dynTableLoad_lany_lptr>(dst, table_ptr);
		addLow<Op_ext_l64>(index);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_dynTableStore_lptr_lany_l64>(
		vm::opargs::StackLocalPtr table_ptr,
		vm::opargs::StackLocalAny src,
		vm::opargs::StackLocal64  index
	) {
		addLow<Op_dynTableStore_lptr_lany>(table_ptr, src);
		addLow<Op_ext_l64>(index);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_dynTableReAlloc_lptr_type_l64>(
		vm::opargs::StackLocalPtr table_ptr,
		vm::opargs::Type          table_type,
		vm::opargs::StackLocal64  new_elem_count
	) {
		addLow<Op_dynTableReAlloc_lptr_type>(table_ptr, table_type);
		addLow<Op_ext_l64>(new_elem_count);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_strOutput_lptr>(vm::opargs::StackLocalPtr arg0) {
		addLow<Op_strOutput_lptr>(arg0);
	}

	// ========= TYPE OPERATIONS ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cast_l8_type>(
		vm::opargs::StackLocal8 arg0, vm::opargs::Type arg1
	) {
		addLow<Op_cast_l8_type>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cast_l16_type>(
		vm::opargs::StackLocal16 arg0, vm::opargs::Type arg1
	) {
		addLow<Op_cast_l16_type>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cast_l32_type>(
		vm::opargs::StackLocal32 arg0, vm::opargs::Type arg1
	) {
		addLow<Op_cast_l32_type>(arg0, arg1);
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_cast_l64_type>(
		vm::opargs::StackLocal64 arg0, vm::opargs::Type arg1
	) {
		addLow<Op_cast_l64_type>(arg0, arg1);
	}

	// ========= MISC ========

	template<>
	void MicroBytecodeBuilder::lower<high::Op_nop>() {
		addLow<Op_nop>();
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_exit>() {
		addLow<Op_exit>();
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_breakpoint>() {
		addLow<Op_breakpoint>();
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_initFromVmValue>() {
		addLow<Op_initFromVmValue>();
	}

	// end of lowering recipes
	// -----------------------

	void MicroBytecodeBuilder::add(const code::Instruction instruction) {
#if (BUILD_TYPE_DEV_DEBUG)
		current_high_instruction_representation = code::instructionToString(instruction);
#endif
		std::visit(
			[&]<typename I>(const I& i) {
				std::apply([&](auto... args) { lower<I>(args...); }, i.argsAsTuple());
			},
			instruction
		);
	}
}
