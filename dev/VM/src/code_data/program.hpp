/**
 * @file code.hpp
 */
#pragma once

#include <filesystem/file.hpp>
#include <base/stable_hashmap.hpp>
#include <base/string_id.hpp>
#include <core/process/type_metadata/type_metadata.hpp>
#include "base/optional.hpp"
#include "core/process/type_metadata/type.hpp"
#include "instruction.hpp"
#include <vector>

namespace vm {
	using ByteCode = std::vector<Fix8Instruction>;

	/**
	 * @brief Function data.
	 */
	struct FuncData {
		ByteCode bc;
		usize    stack_size;
		usize    arg_size;
		usize    next_arg_size;
		usize    ret_size;
	};

	/**
	 * @brief Representation of the program VM runs.
	 * Parser creates this structure from a list of ParsedFile structures after validation.
	 * Executor uses it to execute the code.
	 */
	class VMProgram {
	public:
		// VMProgram() = default;    
		// VMProgram(const VMProgram& other);
    	// VMProgram& operator=(const VMProgram& other);

		base::Optional<CRef<FuncData>>     getFuncByName(base::StrID name) const;
		base::Optional<CRef<vm::FuncData>> getFuncByID(usize id) const;

		base::Optional<CRef<Type>> getTypeByName(base::StrID name) const;
		CRef<Type>                 getTypeByID(TypeID id) const;

		bool addFunction(const base::StrID& func_name, const FuncData& func);

		usize getNumberOfFunctions() const;

		// private:
		base::StableHashMap<base::StrID, usize> func_name_to_id;
		base::StableVector<FuncData>            functions;
		// std::vector<FuncData>            functions;
		TypeMetadata                            type_metadata;
	};
}
