#pragma once

#include "logger.hpp"

#include <filesystem/file.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/loader/compiler/compiler.hpp>
#include <vm/loader/parser/elements.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <expected>
#include <vector>

namespace vm::loader {
	/**
	 * @class Loader
	 * @brief Class, that allows for loading programs in multiple forms.
	 */
	class Loader final {
		/**
		 * @brief The representation of the FatBytecode program.
		 */
		/**
		 * @brief The validated high-level (fat bytecode) representation of the program.
		 * This object is incrementally updated with new, validated code. It is initialized with the
		 * VM's built-in types.
		 */
		code::ValidProgram validated_high_program = code::ValidProgram::withBuiltins();

		/**
		 * @brief The stateful compiler instance for this loader.
		 * It manages the low-level program representation (`LowVMProgram`) and contains
		 * the necessary context to perform compilation of newly added functions.
		 */
		compiler::Compiler compiler{};

		/**
		 * @brief Parses a list of files and returns an intermediate program representation.
		 * @return Either the parsed `CodeCollection` on success, or a `LoaderLogger` with parsing
		 * errors on failure.
		 */
		std::expected<code::CodeCollection, LoaderLogger> parseFiles(
			const std::vector<fs::File>& files
		);

	public:
		explicit Loader() = default;

		/**
		 * @brief Injects new code from given file paths to the current program state and
		 * returns a low-level program representation of the current loader state.
		 */
		std::expected<CRef<vm::low::LowVMProgram>, LoaderLogger> loadAndCompile(
			const std::vector<fs::File>& file_path
		);

		/**
		 * @brief Injects new code from a given high-level code representation, returns a
		 * low-level program representation of the current loader state.
		 */
		std::expected<CRef<vm::low::LowVMProgram>, LoaderLogger> loadAndCompile(
			const code::CodeCollection& code_collection
		);
	};
}
