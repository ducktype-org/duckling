/**
 * @file program.hpp
 */
#pragma once

#include "base/stable_type_id_name_map.hpp"
#include "instruction.hpp"
#include <base/stable_hashmap.hpp>
#include <utility>
#include <vm/core/process/type_metadata/type_metadata.hpp>

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
	class LowVMProgram {
	public:
		LowVMProgram(const std::vector<FuncData>& functions, TypeMetadata types):
			  types(std::move(types)) {
			for (const auto& func: functions) addFunction(func);
		}

		base::Optional<CRef<FuncData>> funcAtMaybe(base::StrID name) const;
		base::Optional<CRef<FuncData>> funcAtMaybe(usize id) const;

		base::Optional<CRef<Type>> typeAtMaybe(base::StrID name) const;
		base::Optional<CRef<Type>> typeAtMaybe(TypeID id) const;
		CRef<Type>                 typeAt(TypeID id) const;

		bool addFunction(const FuncData& func);

		usize getNumberOfFunctions() const;

	private:
		base::StableTypeIdNameMap<FuncData, usize> functions;
		TypeMetadata                               types;
	};
}
