/**
 * @file low_program.hpp
 */
#pragma once

#include "instruction.hpp"

#include <base/pointers/box.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::loader::compiler {
	class Compiler;
}

namespace vm::low {
	using MicroBytecode = std::vector<MicroInstruction>;

	/**
	 * @brief Micro bytecode representation of function data.
	 */
	struct LowFuncData {
		base::StrID           name;
		MicroBytecode         bc;
		usize                 local_stack_size;
		usize                 arg_size;
		// total summed size of all return values
		usize                 ret_size;
		std::vector<TypeCRef> parameters;
		std::vector<TypeCRef> result_type;
	};

	/**
	 * @brief Micro bytecode representation of global data.
	 */
	struct LowGlobalData {
		TypeCRef                    type;
		base::Optional<base::StrID> ctor_name;
		base::Optional<base::StrID> dtor_name;
	};

	/**
	 * @brief Micro bytecode representation of an extern C function.
	 */
	struct LowExternCFunction {
		base::StrID name;
		void (*function_pointer)(std::byte*, std::byte*) = nullptr;
		usize                 parameter_size_sum;
		std::vector<TypeCRef> parameters;
		std::vector<TypeCRef> result_type;
	};

	/**
	 * @brief Representation of the micro bytecode program which the VM runs.
	 * This is the final form of bytecode produced by the loader module which is executable by
	 * `VMThread`.
	 *
	 * @note This structure can only be created by the compiler.
	 * @note The program represented by this structure is always valid as it was verified in the
	 * loading stage.
	 *
	 * @note The order of functions in the `ObjIdNameMap<LowFuncData, usize>` is important, as the
	 * `ID` of the function used when performing function calls is the index in the `std::vector`
	 * (used internally in `ObjIdNameMap`). Similar holds for `ObjIdNameMap<LowGlobalData,
	 * GlobalDataID>` - global data.
	 */
	class LowVMProgram final {
	public:
		friend class vm::loader::compiler::Compiler;

		const TypeMetadata& getTypes() const { return *types; }

		const ObjIdNameMap<LowFuncData, usize>& getFunctions() const { return functions; }

		const ObjIdNameMap<LowExternCFunction>& getExternCFunctions() const {
			return extern_c_functions;
		}

		const ObjIdNameMap<LowGlobalData, GlobalDataID>& getGlobals() const { return global_data; }

		const base::HashMap<u64, base::StrID>& getMethodNamePool() const {
			return method_name_pool;
		}

	private:
		LowVMProgram()                                  = default;
		Box<TypeMetadata>                         types = makeBox<TypeMetadata>();
		ObjIdNameMap<LowFuncData, usize>          functions{};
		ObjIdNameMap<LowExternCFunction>          extern_c_functions{};
		ObjIdNameMap<LowGlobalData, GlobalDataID> global_data{};
		// Contains all method names in the program. It's used by the executor to determine the
		// names of called functions.
		base::HashMap<u64, base::StrID> method_name_pool{};
	};

}
