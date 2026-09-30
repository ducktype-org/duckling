/**
 * @file compilation_mode.hpp
 * @brief Describes the kind of function that is being compiled.
 *
 * A function is validated and then lowered with the same mode, so the mode is carried by the
 * resulting `ValidFunction` rather than threaded through the compiler as a separate argument.
 */
#pragma once

#include <variant>

namespace vm::code {
	/**
	 * @brief A regular function of the loaded program.
	 */
	struct NormalFunction {};

	/**
	 * @brief The synthetic `vm_start_function` / `runFunction` wrapper, built at run time.
	 */
	struct StartFunction {};

	/**
	 * @brief The kind of function currently being validated and lowered.
	 */
	using CompilationMode = std::variant<NormalFunction, StartFunction>;
}
