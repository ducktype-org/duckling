// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/api/data/execution_config.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/flags.hpp>
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
			api::ExecutionConfig                   config
		);

		InstructionFlag getFlagsForFunction(const base::StrID function) const;

	private:
		// The sum of all InstructionFlags for the instructions which may be executed
		// (directly or indirectly) by a given function.
		// @note The use of `InstructionFlag` instead of `FunctionFlag` here is not a mistake.
		base::HashMap<base::StrID, InstructionFlag> flags_in_functions;
	};
}
