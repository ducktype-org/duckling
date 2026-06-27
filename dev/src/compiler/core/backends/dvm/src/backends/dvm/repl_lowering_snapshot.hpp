#pragma once

#include <base/types/ints.hpp>

namespace compiler::backend_vm {
	namespace internal {
		class ProgramLoweringContext;
	}

	/**
	 * @brief Opaque snapshot/cursor for incremental REPL lowering.
	 *
	 * Purpose: This struct acts as a private, immutable stamp of the lowering context's state,
	 * allowing the REPL to track which types, globals, and functions have been newly lowered
	 * since a given point. Only the lowering context can construct or mutate it.
	 */
	struct LoweredEntitiesSnapshot final {
	public:
		// Getters strictly used for debug purposes.
		[[nodiscard]] usize loweredTypeCount() const { return lowered_type_count; }

		[[nodiscard]] usize loweredGlobalCount() const { return lowered_global_count; }

		[[nodiscard]] usize loweredFunctionCount() const { return lowered_function_count; }

		[[nodiscard]] usize extraBytecodeFunctionCount() const {
			return extra_bytecode_function_count;
		}

	private:
		friend class internal::ProgramLoweringContext;
		friend class ReplDVMCodeBuilder;

		LoweredEntitiesSnapshot(
			usize type_count, usize global_count, usize function_count, usize extra_function_count
		):
			  lowered_type_count(type_count),
			  lowered_global_count(global_count),
			  lowered_function_count(function_count),
			  extra_bytecode_function_count(extra_function_count) {}

		usize lowered_type_count            = 0;
		usize lowered_global_count          = 0;
		usize lowered_function_count        = 0;
		usize extra_bytecode_function_count = 0;
	};
}
