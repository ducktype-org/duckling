#pragma once

#include "logger.hpp"

#include <filesystem/file.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/loader/parser/elements.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <expected>
#include <vector>

namespace vm::loader {
	/**
	 * @brief Loader class, that allows for loading programs in multiple forms.
	 */
	class Loader final {
		code::ValidProgram program = code::ValidProgram::withBuiltins();

		/**
		 * @brief Parses a list of files, returns an intermediate loader-only program
		 * representation.
		 */
		std::expected<code::CodeCollection, LoaderLogger> loadFiles(const std::vector<fs::File>& files
		);

	public:
		explicit Loader() = default;

		/**
		 * @brief Injects new code from given file paths to the current program state and
		 * returns a low-level program representation of the current loader state.
		 */
		std::expected<vm::low::LowVMProgram, LoaderLogger> getProgram(
			const std::vector<fs::File>& file_path
		);

		/**
		 * @brief Injects new code from a given high-level code representation, returns a
		 * low-level program representation of the current loader state.
		 */
		std::expected<vm::low::LowVMProgram, LoaderLogger> getProgram(
			const code::CodeCollection& code_collection
		);
	};
}
