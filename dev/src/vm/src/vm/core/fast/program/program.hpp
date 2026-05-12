#pragma once

#include "type.hpp"

#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::fast {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(FunctionID);

	namespace reloc {
		struct ExternCFunc {
			base::StrID         name;
			std::vector<TypeID> arg_types;
		};

		struct Function {
			base::StrID name;

			/// The maximum size of the local variables on stack required by the function frame.
			Bytes local_stack_size;
			// The total summed size of all return values.
			usize ret_size;

			std::vector<TypeID> arg_types;
			std::vector<TypeID> result_types;
		};

		struct Program {
			ObjIdNameMap<
			ObjIdNameMap<Type, TypeID>         types{};
			ObjIdNameMap<ExternCFunc>          extern_c_functions{};
			ObjIdNameMap<Function, FunctionID> functions{};
		};
	}

	namespace exec {
		// Box<TypeMetadata>                         types = makeBox<TypeMetadata>();
		// ObjIdNameMap<LowFuncData, usize>          functions{};
		// StableObjIdNameMap<LowExternCFunction>    extern_c_functions{};
		// ObjIdNameMap<LowGlobalData, GlobalDataID> global_data{};
		// Bytes                                     global_buffer_size = Bytes(0);
		// usize                                     global_count       = 0;

		// // Contains all method names in the program. It's used by the executor to determine the
		// // names of called functions.
		// base::HashMap<u64, base::StrID> method_name_pool{};
	}
}
