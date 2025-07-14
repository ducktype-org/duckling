/**
 * @file low_program.hpp
 */
#pragma once

#include "instruction.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <iostream>
#include <string>
#include <utility>

namespace vm::low {
	using ByteCode = std::vector<Fix8Instruction>;

	/**
	 * @brief Function data.
	 */
	struct FuncData {
		base::StrID name;
		ByteCode    bc;
		usize       local_stack_size;
		usize       arg_size;
		usize       ret_size;
	};

	/**
	 * @brief Global data.
	 */
	struct GlobData {
		TypeCRef                    type;
		base::Optional<std::string> ctor_name;
		base::Optional<std::string> dtor_name;
	};

	/**
	 * @brief Representation of the program VM runs.
	 * Parser creates this structure from a list of ParsedFile structures after validation.
	 * Executor uses it to execute the code.
	 * @note In the future, this class will use micro bytecode instead.
	 *
	 * @note The order of functions in the `std::vector<FuncData>` is important, as the `ID` of
	 * the function in the function calls is the index in this vector. Similar holds for
	 * `std::vector<code::GlobalData>` - global data.
	 */
	struct LowVMProgram {
		LowVMProgram(
			Box<TypeMetadata>                      types,
			const std::vector<FuncData>&           functions,
			const std::vector<code::GlobalData>&   global_data,
			const base::HashMap<i32, base::StrID>& method_name_pool
		):
			  types(std::move(types)),
			  method_name_pool(method_name_pool) {
			std::unordered_map<std::string, std::string> ctor_functions;

			for (const auto& func: functions) {
				this->functions.insert(func, func.name);
				std::cerr << "Inserting: " << func.name.strView() << "\n";

				// Detect if it's a global constructor by prefix
				constexpr std::string_view prefix = "_ctor_GLOBAL_";
				std::string                fname  = func.name.str();
				if (fname.starts_with(prefix)) {
					std::string global_name(fname.substr(prefix.size()));
					ctor_functions[global_name] = func.name.str();
				}
			}

			for (const auto& global: global_data) {
				GlobData data{ .type      = this->types->at(global.type),
					           .ctor_name = global.ctor_name,
					           .dtor_name = global.dtor_name };

				std::string gname = global.name.str.str();
				if (ctor_functions.contains(gname)) {
					data.ctor_name = ctor_functions[gname];
					std::cerr << "Linked ctor to global: " << gname << " <- "
							  << data.ctor_name.value() << "\n";
				}

				this->global_data.insert(data, global.name);
				std::cerr << "Inserting: " << gname << "\n";
			}
		}

		Box<TypeMetadata>                          types;
		StableObjIdNameMap<FuncData, usize>        functions;
		StableObjIdNameMap<GlobData, GlobalDataID> global_data;
		// Contains all method names in the program. It's used by the executor to determine the
		// names of called functions.
		base::HashMap<i32, base::StrID> method_name_pool;
	};
}
