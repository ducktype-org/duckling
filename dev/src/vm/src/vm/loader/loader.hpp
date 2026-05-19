#pragma once

#include "logger.hpp"

#include <filesystem/file.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/loader/compiler/compiler.hpp>

#include <expected>

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
		static std::expected<code::CodeCollection, LoaderLogger> parseFiles(
			const std::vector<fs::File>& files
		);

	public:
		explicit Loader();

		/**
		 * @brief Returns a pointer to the low-level program representation of the current loader
		 * state.
		 * @note The reference will be valid as long as the Loader itself and it's value updates on
		 * loads calls.
		 */
		CRef<vm::low::LowVMProgram> getProgram() const;

		/**
		 * @brief Injects new code from given file paths to the current program state.
		 */
		std::expected<void, LoaderLogger> loadAndCompile(
			const std::vector<fs::File>& file_path, bool attach_mapping
		);

		/**
		 * @brief Injects new code from a given high-level code representation.
		 */
		std::expected<void, LoaderLogger> loadAndCompile(
			const code::CodeCollection& code_collection, bool attach_mapping
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

		struct FatBytecodePosition {
			base::StrID function_name;
			usize       instruction_index;
		};

		enum MappingException {
			MissingMapping,
			NoFunction,
		};

		std::expected<FatBytecodePosition, MappingException> mapLowVMProgramPositionToCodeCollectionPosition(
			std::variant<u64, base::StrID> function_identifier, usize instruction_index
		) const;

		std::expected<FatBytecodePosition, MappingException> mapLowVMProgramPositionToCodeCollectionPosition(
			low::LowCodePosition position
		) const;

		std::expected<base::Optional<dia::SourcePosition>, MappingException> mapCodeCollectionPositionToFilePosition(
			base::StrID function_id, usize instruction_index
		) const;

		std::expected<base::Optional<dia::SourcePosition>, MappingException> mapCodeCollectionPositionToFilePosition(
			FatBytecodePosition position
		) const;
	};
}
