/**
 * @file program.hpp
 */
#pragma once

#include "instruction.hpp"
#include <base/stable_hashmap.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

namespace vm::low {
	using ByteCode = std::vector<Fix8Instruction>;

	/**
	 * @brief Function data.
	 */
	struct FuncData {
		base::StrID name;
		ByteCode    bc;
		usize       stack_size;
		usize       arg_size;
		usize       next_arg_size;
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
	class LowVMProgram {
	public:
		LowVMProgram(const std::vector<FuncData>& functions, Box<TypeMetadata> type_metadata):
			  type_metadata(std::move(type_metadata)) {
			for (auto& func: functions) addFunction(func.name, func);
		}

		base::Optional<CRef<FuncData>> getFuncByName(base::StrID name) const;
		base::Optional<CRef<FuncData>> getFuncByID(usize id) const;

		base::Optional<CRef<Type>> getTypeByName(base::StrID name) const;
		CRef<Type>                 getTypeByID(TypeID id) const;

		bool addFunction(const base::StrID& func_name, const FuncData& func);

		usize getNumberOfFunctions() const;

		base::StableHashMap<base::StrID, usize> func_name_to_id;
		base::StableVector<FuncData>            functions;
		Box<TypeMetadata>                       type_metadata;
	};
}
