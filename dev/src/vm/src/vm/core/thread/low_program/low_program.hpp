/**
 * @file low_program.hpp
 */
#pragma once

#include "instruction.hpp"

#include "base/ints.hpp"
#include "base/maps.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <utility>

namespace vm::low {
	using MicroByteCode = std::vector<MicroInstruction>;

	/**
	 * @brief Micro bytecode representation of function data.
	 */
	struct LowFuncData {
		base::StrID           name;
		MicroByteCode         bc;
		usize                 local_stack_size;
		usize                 arg_size;
		usize                 ret_size;
		std::vector<TypeCRef> parameters;
		TypeCRef              result_type;
	};

	/**
	 * @brief Micro bytecode representation of Global data.
	 */
	struct LowGlobalData {
		TypeCRef                    type;
		base::Optional<base::StrID> ctor_name;
		base::Optional<base::StrID> dtor_name;
	};

	/**
	 * @brief Representation of the program VM runs.
	 * TODOP: FIx this comment.
	 * Parser creates this structure from a list of ParsedFile structures after validation.
	 * Executor uses it to execute the code.
	 * @note In the future, this class will use micro bytecode instead.
	 *
	 * // TODOP: Fix this comment as well.
	 * @note The order of functions in the `std::vector<FuncData>` is important, as the `ID` of
	 * the function in the function calls is the index in this vector. Similar holds for
	 * `std::vector<code::GlobalData>` - global data.
	 *
	 * TODOP: Represents the MicroBytecode program. It's always valid.
	 * TODOP: This is just a simple container for the low representation of the program
	 */
	class LowVMProgram {
	public:
		LowVMProgram();

		Box<TypeMetadata>                         types;
		ObjIdNameMap<LowFuncData, usize>          functions;
		ObjIdNameMap<LowGlobalData, GlobalDataID> global_data;
		// Contains all method names in the program. It's used by the executor to determine the
		// names of called functions.
		base::HashMap<u64, base::StrID> method_name_pool;
	};
}
