#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

namespace vm::code {
	/**
	 * @brief Performs function code validation in the given context and extracts reachable code.
	 */
	Function validateAndExtractReachableCode(
		const ObjIdNameMap<TypeOfData>& type_context,
		const TypeMetadata&             type_metadata,
		const ObjIdNameMap<GlobalData>& globals_map,
		const Function&                 function
	);
}
