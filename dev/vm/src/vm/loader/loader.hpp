#pragma once

#include "logger.hpp"

#include <filesystem/file.hpp>

#include <base/stable_type_id_name_map.hpp>

#include <vm/code/builders/builders.hpp>
#include <vm/code/code.hpp>
#include <vm/code/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/loader/parser/elements.hpp>

#include <expected>

namespace vm::loader {
	/**
	 * @brief Loader-only program representation. It allows for dynamic function and type insertion.
	 */
	class Program final {
	public:
		Program(const Program&)            = delete;
		Program(Program&&) noexcept        = default;
		Program& operator=(const Program&) = delete;
		Program& operator=(Program&&)      = default;

		static std::expected<Program, LoaderLogger> from(const code::CodeCollection& code_collection
		);
		const base::StableTypeIdNameMap<code::Function>& funcMap() const;

		Box<TypeMetadata> produceTypeMetadata() const;

	private:
		Program() = default;

		void insertTypes(const std::vector<code::TypeOfData>& types, LoaderLogger& logger);
		void
			insertFunctions(const std::vector<code::Function>& new_functions, LoaderLogger& logger);

		base::StableTypeIdNameMap<code::Function> functions;
		Box<TypeMetadata>                         type_metadata = makeBox<TypeMetadata>();
		std::vector<code::TypeOfData>             types;  /// used only for error messages
		code::builders::TypesContext<>            types_context_adding;
	};

	/**
	 * @brief Loader class, that allows for loading programs in multiple forms.
	 */
	class Loader final {
		bool validate_program;

	public:
		explicit Loader(bool validate_program);

		/**
		 * @brief Parses the file, returns a
		 * low-level program representation.
		 */
		static std::expected<low::LowVMProgram, LoaderLogger> getProgram(const fs::FilePath& file);

		/**
		 * @brief Parses a list of files, returns a
		 * low-level program representation.
		 */
		static std::expected<low::LowVMProgram, LoaderLogger>
			getProgram(const std::vector<fs::FilePath>& files);

		/**
		 * @brief Builds LowVMProgram from high-level code representation, returns a
		 * low-level program representation.
		 */
		static std::expected<low::LowVMProgram, LoaderLogger>
			getProgram(const code::CodeCollection& code_collection);
	};
}
