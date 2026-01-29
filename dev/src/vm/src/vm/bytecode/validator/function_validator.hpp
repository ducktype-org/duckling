#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/type_context.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

namespace vm::code::detail {
	/**
	 * @brief Performs function code validation in the given context and extracts reachable code.
	 */
	Function validateAndExtractReachableCode(
		const ObjIdNameMap<vm::code::type::Type>&        types,
		const ObjIdNameMap<GlobalData>&                  globals_map,
		const base::HashMap<base::StrID, FuncSignature>& signatures,
		const ObjIdNameMap<ExternalCFunction>&           ext_c_functions,
		const Function&                                  function
	);
}
