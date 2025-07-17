#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

namespace vm::code {
	struct Signature {
		std::vector<base::StrID> parameters;
		base::StrID              result_type;
	};

	/**
	 * @brief Performs function code validation in the given context and extracts reachable code.
	 */
	Function validateAndExtractReachableCode(
		const StableObjIdNameMap<TypeOfData>& type_context,
		const base::HashMap <base::StrID, Signature>& signatures,
		const TypeMetadata&                   type_metadata,
		const StableObjIdNameMap<GlobalData>& globals_map,
		const Function&                       function
	);
}
