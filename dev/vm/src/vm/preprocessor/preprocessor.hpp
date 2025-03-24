#pragma once

#include "diagnostic/logger.hpp"
#include "vm/preprocessor/parser/elements.hpp"
#include "vm/program/program.hpp"
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>

namespace vm {
	using PosMap = base::HashMap<
		std::variant<
			const vm::program::TypeOfData*,
			const program::Function*,
			const program::Instruction*,
			const opargs::OpCodeArg*>,
		dia::SourcePosition>;

	class Preprocessor {
	private:
		bool validate_program;

		std::expected<low::LowVMProgram, dia::Logger> getProgram(
			const std::vector<program::CodeFile>& code_files, base::Optional<const PosMap&> pos_map
		);

	public:
		Preprocessor(bool validate_program);

		/**
		 * @brief Parses the file, returns the representation of the program with type metadata.
		 *
		 * @param file
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, dia::Logger> getProgram(const fs::FilePath& file);


		/**
		 * @brief Parses a list of files, returns the representation of the program with type
		 * metadata.
		 *
		 * @param files
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, dia::Logger>
			getProgram(const std::vector<fs::FilePath>& files);
	};
}
