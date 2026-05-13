#pragma once

#include <vm/bytecode/flags.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code {
	/**
	 * @brief A simple container to keep track of the flags of instructions and functions,
	 * for the purpose of verification.
	 * @note `FlagContext` always contains a valid mapping of instruction/function flags if
	 * `insertAndValidate` didn't throw any errors.
	 */
	class FlagContext final {
	public:
		/**
		 * @brief Inserts new functions and computes their flags based on their content
		 * and the flags of previously computed functions.
		 */
		void insertAndValidate(
			const std::vector<Function>&           new_functions,
			const ObjIdNameMap<GlobalData>&        globals,
			const ObjIdNameMap<ExternalCFunction>& ext_c_functions,
			api::ExecutionConfig config
		);

	private:
		// The sum of all InstructionFlags for the instructions which may be executed
		// (directly or indirectly) by a given function.
		// @note The use of `InstructionFlag` instead of `FunctionFlag` here is not a mistake.
		base::HashMap<base::StrID, InstructionFlag> flags_in_functions;
	};
}
