/**
 * @file compilation_mode.hpp
 * @brief Describes the kind of function that is being compiled.
 *
 * The same mode drives both validation and lowering, so it lives here rather than in the
 * validator: a function is validated and then compiled in one of these modes.
 */
#pragma once

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/loader/bytecode_pos.hpp>

#include <deque>
#include <variant>

namespace vm {
	class SafeVMValue;
}

namespace vm::code {
	/**
	 * @brief A regular function of the loaded program.
	 */
	struct NormalFunction {};

	/**
	 * @brief A runtime expression evaluated against a paused thread.
	 *
	 * Its locals live in the frame the expression was called from, so validation and lowering
	 * resolve names through the captured call stack rather than the expression's own frame.
	 */
	struct Expression {
		base::CRef<std::deque<Box<SafeVMValue>>> vm_values;
		const Frame*                             call_stack_base;
		usize                                    call_stack_size;

		[[nodiscard]] base::Optional<vm::loader::ValidFuncPosition> getUpcomingHighPosition(
			usize frame_idx
		) const {
			if (frame_idx >= call_stack_size) return std::nullopt;
			const Frame& frame = call_stack_base[frame_idx];

			auto& func          = *frame.current_function;
			auto  low_instr_idx = static_cast<u64>(frame.instr - func.getBc().data());

			auto fat_pos = func.mapLowVMProgramPositionToCodeCollectionPosition(low_instr_idx);
			if (!fat_pos) return std::nullopt;

			return loader::ValidFuncPosition(fat_pos->instruction_index, func.getHighFunc());
		}
	};

	/**
	 * @brief The synthetic `vm_start_function` / `runFunction` wrapper, built at run time.
	 */
	struct StartFunction {};

	/**
	 * @brief The kind of function currently being validated and lowered.
	 */
	using CompilationMode = std::variant<NormalFunction, Expression, StartFunction>;
}
