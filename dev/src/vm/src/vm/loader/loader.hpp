#pragma once

#include "logger.hpp"

#include <filesystem/file.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_program.hpp>

#include <expected>

namespace vm::loader {
	struct FatBytecodePosition {
		base::StrID function_name;
		usize       instruction_index;
	};

	enum class MappingException {
		MissingMapping,
		NoFunction,
	};

	/**
	 * @class Loader
	 * @brief Class, that allows for loading programs in multiple forms.
	 */
	class Loader final {
		/**
		 * @brief The validated high-level (fat bytecode) representation of the program.
		 * This object is incrementally updated with new, validated code. It is initialized with the
		 * VM's built-in types.
		 */
		code::ValidProgram validated_high_program = code::ValidProgram::withBuiltins();

		/**
		 * @brief Parses a list of files and returns an intermediate program representation.
		 * @return Either the parsed `CodeCollection` on success, or a `LoaderLogger` with parsing
		 * errors on failure.
		 */
		static std::expected<code::CodeCollection, LoaderLogger> parseFiles(
			const std::vector<fs::File>& files
		);

	public:
		Loader() = default;

		/**
		 * @brief Injects new code from given file paths to the current program state.
		 */
		std::expected<void, LoaderLogger> loadAndValidate(const std::vector<fs::File>& file_path);

		/**
		 * @brief Injects new code from a given high-level code representation.
		 */
		std::expected<void, LoaderLogger> loadAndValidate(const code::CodeCollection& code_collection
		);

		CRef<code::ValidProgram> getHighProgram() const;

		/**
		 * @brief Parses .dbc files and returns the combined CodeCollection without validation,
		 * or a string error message on failure. Main user is the compiler driver.
		 *
		 * @return Either the parsed `CodeCollection` on success, or a string error message on
		 * failure.
		 */
		static std::expected<code::CodeCollection, std::string> parseCodeCollectionFromFiles(
			const std::vector<fs::File>& files
		);

		std::expected<base::Optional<dia::SourcePosition>, MappingException> mapCodeCollectionPositionToFilePosition(
			FatBytecodePosition position
		) const;

		/**
		 * @brief Gets the first code collection instruction that starts in the provided file line.
		 * @return Either the mapped `FatBytecodePosition` on success, or a nullopt if no such
		 * instruction exists.
		 */
		base::Optional<FatBytecodePosition> mapFileLineToCodeCollectionPosition(
			fs::File file, usize line_number
		) const;
	};
}
