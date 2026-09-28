/**
 * @file compilation_mode.hpp
 * @brief Describes the kind of function that is being compiled.
 *
 */
#pragma once

#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <deque>
#include <variant>

namespace vm {
	class SafeVMValue;
	struct Frame;
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
