#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

namespace vm::code {
	Function validateAndExtractReachableCode(
		const TypeMetadata&                    type_metadata,
		const StableTypeIdNameMap<GlobalData>& globals_map,
		const Function&                        function
	);
}
