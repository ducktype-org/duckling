#pragma once

#include "flag_context.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_function.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/core/safe/type_metadata/type_metadata.hpp>

namespace vm::code::detail {
	/**
	 * @brief Performs function code validation in the given context and extracts reachable code.
	 */
	valid_function::ValidFunction validateAndExtractReachableCode(
		const valid_type::ValidTypeMap&                  types,
		const ObjIdNameMap<GlobalData>&                  globals_map,
		const base::HashMap<base::StrID, FuncSignature>& signatures,
		const ObjIdNameMap<ExternalCFunction>&           ext_c_functions,
		const FlagContext&                               flag_context,
		const ObjIdNameMap<FFIFunction>&                 ffi_functions,
		const Function&                                  function
	);
}
