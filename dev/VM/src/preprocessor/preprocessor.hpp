#pragma once

#include <core/process/type_metadata/type_metadata.hpp>
#include <code_data/program.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>
#include <expected>
#include <validator/validator.hpp>
#include <parser/parser.hpp>

namespace vm {
	class VMProcess;

	// @TODO: static type checking

	/**
	 * @brief Service that loads the program file to the VM.
	 * @TODO: Refactor this to return a program object....
	 */
	class Preprocessor {
		friend VMProcess;

	private:
		bool validate_program;
		validator::Validator validator;
		
		std::expected<vm::VMProgram, std::string> Preprocessor::changeParsedProgramToVMProgram(const ParsedProgram& parsed_program);
		
		Preprocessor(VMProcess& process, bool validateProgram);

	public:
		template<class... DynamicServices>
		friend class ServiceManagerDef;

		/**
		 * @brief Parses the file, returns the representation of the program with type metadata.
		 *
		 * @param file
		 * @return std::expected<vm::VMProgram, std::string>
		 */
		std::expected<vm::VMProgram, std::string> getProgram(const fs::FilePath& file);


		/**
		 * @brief Parses a list of files, returns the representation of the program with type metadata.
		 *
		 * @param files
		 * @return std::expected<vm::VMProgram, std::string>
		 */
		std::expected<vm::VMProgram, std::string> getProgram(const std::vector<fs::FilePath>& files);



	};
}
