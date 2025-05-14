#pragma once

#include "logger.hpp"

#include <filesystem/file.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/loader/parser/elements.hpp>
#include <vm/utils/stable_type_id_name_map.hpp>

#include <expected>
#include <vector>

namespace vm::loader {
	/**
	 * @brief Loader-only program representation. It allows for dynamic function and type insertion.
	 */
	class Program final {
	public:
		Program()                                                             = default;
		Program(const Program&)                                               = delete;
		Program(Program&&) noexcept                                           = default;
		Program&                                    operator=(const Program&) = delete;
		Program&                                    operator=(Program&&)      = default;
		static std::expected<Program, LoaderLogger> from(const code::CodeCollection& code_collection
		);

		const StableTypeIdNameMap<code::Function>&   funcMap() const;
		const StableTypeIdNameMap<code::TypeOfData>& typeMap() const;
		const code::builders::GlobalDataMap&         globalMap() const;

		Box<TypeMetadata> produceTypeMetadata() const;

		/**
		 * @brief Inserts new code to a current program state. Can be called multiple times with
		 * the same code collection object.
		 */
		void insertCode(
			const std::vector<code::CodeCollection>& new_code_collections, LoaderLogger& logger
		);

		/**
		 * @brief Inserts a type. Can be called multiple times with the same type object.
		 */
		void insertTypes(const std::vector<code::TypeOfData>& new_types, LoaderLogger& logger);

		/**
		 * @brief Inserts a function. Cannot be called multiple times with the same function
		 * object.
		 */
		void insertFunctions(const std::vector<code::Function>& new_functions, LoaderLogger& logger);

		/**
		 * @brief Inserts a global. Cannot be called multiple times with the same global data
		 * object.
		 */
		void insertGlobals(const std::vector<code::GlobalData>& new_globals, LoaderLogger& logger);

		code::builders::TypeContext getTypeContext() const;

	private:
		StableTypeIdNameMap<code::Function> functions;
		code::builders::GlobalDataMap       globals_map;
		code::builders::TypeContextBuilder  type_context_builder;
	};

	/**
	 * @brief Loader class, that allows for loading programs in multiple forms.
	 */
	class Loader final {
		bool    validate_program;
		Program program;

		/**
		 * @brief Parses a list of files, returns an intermediate loader-only program
		 * representation.
		 */
		std::expected<code::CodeCollection, LoaderLogger> loadFiles(
			const std::vector<fs::FilePath>& files
		);

	public:
		explicit Loader(bool validate_program);

		/**
		 * @brief Injects new code from given file paths to the current program state and
		 * returns a low-level program representation of the current loader state.
		 */
		std::expected<vm::low::LowVMProgram, LoaderLogger> getProgram(
			const std::vector<fs::FilePath>& file_path
		);

		/**
		 * @brief Injects new code from a given high-level code representation, returns a
		 * low-level program representation of the current loader state.
		 */
		std::expected<vm::low::LowVMProgram, LoaderLogger> getProgram(
			const std::vector<code::CodeCollection>& code_collection
		);
	};
}
