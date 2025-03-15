#pragma once

#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>

namespace vm {
	/**
	 * @brief Object that loads the program file to the VM.
	 * Current preprocessor pipeline is as follows:
	 * 1) Parse the program from a given list of files and create
	 *    a ParsedProgram object, enriched in source positions
	 *    of every opcode, type etc.
	 * 2) Perform static verification of the code using the Validator module
	 * 3) Convert ParsedProgram object intoLowVMProgram which will be used to
	 *    execute the code.
	 */
	class Preprocessor {
	private:
		bool validate_program;

		/**
		 * @brief Changes the program representation from parser representation
		 * (with source positions) to a program format executable by the VM.
		 *
		 * @param parsed_program
		 * @todo This should bo moved to the core/thread or be a method on vm::VMProgram
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, std::string>
			changeParsedProgramToLowVMProgram(const parser::ParsedProgram& parsed_program);

	public:
		Preprocessor(bool validate_program);

		/**
		 * @brief Parses the file, returns the representation of the program with type metadata.
		 *
		 * @param file
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, std::string> getProgram(const fs::FilePath& file);


		/**
		 * @brief Parses a list of files, returns the representation of the program with type
		 * metadata.
		 *
		 * @param files
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, std::string>
			getProgram(const std::vector<fs::FilePath>& files);
	};
}
