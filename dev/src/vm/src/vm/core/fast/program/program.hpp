#pragma once

#include <vm/utils/stable_obj_id_name_map.hpp>
#include "type.hpp"
namespace vm::fast {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(FunctionID);
    struct ProgramBase {
        ObjIdNameMap<Type, TypeID> types{};
		StableObjIdNameMap<LowExternCFunction>    extern_c_functions{};
    };
    namespace reloc {

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
