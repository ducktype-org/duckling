/**
 * @file code.hpp
 */
#pragma once

#include "instruction.hpp"

#include <base/stable_hashmap.hpp>
#include <base/stable_type_id_name_map.hpp>

#include <vm/core/process/type_metadata/type_metadata.hpp>

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
	 * @brief Representation of the program VM runs.
	 * Parser creates this structure from a list of ParsedFile structures after validation.
	 * Executor uses it to execute the code.
	 * @note In the future, this class will use micro bytecode instead.
	 *
	 * @note It's guaranteed to contain main, if validator is enabled.
	 */
	struct LowVMProgram {
		LowVMProgram(const std::vector<FuncData>& functions, Box<TypeMetadata> types):
			  types(std::move(types)) {
			for (const auto& func: functions) this->functions.insert(func, func.name);
		}

		base::StableTypeIdNameMap<FuncData, usize> functions;
		Box<TypeMetadata>                          types;
	};
}
