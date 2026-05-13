#pragma once

#include "ids.hpp"
#include "type.hpp"

#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/program/instructions/relocatable.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::fast {

	struct ExternCFunction {
		base::StrID name;
		void (*function_pointer)(std::byte*, std::byte*) = nullptr;
		Bytes                    parameter_size_sum;
		std::vector<TypeCRef>    parameter_types;
		base::Optional<TypeCRef> result_type;
	};

	struct FunctionInfo {
		base::StrID name;
		FunctionID  id;

		/// The maximum size of the local variables on stack required by the function frame.
		Bytes local_stack_size;
		// The total summed size of all return values.
		usize ret_size;

		std::vector<TypeID> arg_types;
		std::vector<TypeID> result_types;
	};

	struct GlobalData {
		/// The name of the global variable.
		base::StrID name;
		/// The type of the global variable.
		TypeCRef type;
		/// The offset of the global variable's data in the global buffer.
		u64 global_buffer_offset;
	};

	/**
	 * @brief The base program structure, containing all the information about the program, except
	 * the actual instruction data.
	 * @note Relocated representation contains pointers to this structure, so after modifications it
	 * has to be re-relocated.
	 */
	struct ProgramBase {
		ObjIdNameMap<GlobalData, GlobalDataID> global_data{};
		TypeCollection                         types{};
		ObjIdNameMap<ExternCFunction>          extern_c_functions{};
		ObjIdNameMap<FunctionInfo, FunctionID> functions{};

		Bytes global_buffer_size = Bytes(0);
	};

	namespace reloc {
		struct RelocFunction {
			FunctionID               id;
			std::vector<Instruction> data;
		};

		using RelocFunctionCollection = std::vector<RelocFunction>;
	}

	namespace exec {
		struct ExecFunction {
			FunctionID               id;
			std::vector<Instruction> data;
		};

		using ExecFunctionCollection = std::vector<ExecFunction>;
	}

}
