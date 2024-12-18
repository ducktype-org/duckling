/**
 * @file code.hpp
 *
 */
#pragma once

#include <filesystem/file.hpp>
#include <base/stable_hashmap.hpp>
#include <base/string_id.hpp>
#include <core/process/type_metadata/type_metadata.hpp>
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
	 * Parser creates this structure from the text file and the Executor uses it to execute the
	 * code.
	 */
	class VMProgram {
		friend class OpFuns;

	public:
		VMProgram() = default;

		// @todo: These methods should be made private/accessible only from preprocessor.

		// cpp::result<VMProgram, std::string> includeFile(const fs::FilePath& file) const {
		// 	auto code = assemble::assemble(file, type_meta_data);

		// 	// @TODO: Report an error
		// 	if (code.has_error()) return false;

		// 	code.
		// }

		static cpp::result<VMProgram, std::string> assemble(const fs::FilePath& file);
		static cpp::result<vm::VMProgram, std::string>
			assemble(const std::vector<fs::FilePath>& file);

	private:
		base::StableHashMap<base::StrID, FuncData> name_to_fun;
		std::vector<FuncData> functions;

		TypeMetadata type_metadata;
	};
}
