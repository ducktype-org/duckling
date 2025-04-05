#pragma once

#include "logger.hpp"

#include <diagnostic/logger.hpp>
#include <diagnostic/source_position.hpp>
#include <filesystem/file.hpp>

#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/stable_type_id_name_map.hpp>

#include <vm/code/builders/builders.hpp>
#include <vm/code/code.hpp>
#include <vm/code/element_base.hpp>
#include <vm/code/type_of_data.hpp>
#include <vm/core/process/type_metadata/type.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/loader/parser/elements.hpp>

#include <expected>

namespace vm {
	/**
	 * @brief Loader-only program representation. It allows for dynamic function and type insertion.
	 */
	class Program final {
	public:
		Program(const Program&)            = delete;
		Program(Program&&) noexcept        = default;
		Program& operator=(const Program&) = delete;
		Program& operator=(Program&&)      = default;

		static std::expected<Program, LoaderLogger> from(
			const std::vector<code::Function>& functions, const std::vector<code::TypeOfData>& types
		);
		const base::StableTypeIdNameMap<code::Function>& funcMap() const;

		Box<TypeMetadata> produceTypeMetadata() const;

	private:
		Program() = default;

		void insertTypes(const std::vector<code::TypeOfData>& types, LoaderLogger& logger);
		void insertFunctions(const std::vector<code::Function>& functions, LoaderLogger& logger);

		base::StableTypeIdNameMap<code::Function> functions;
		Box<TypeMetadata>                         type_metadata = makeBox<TypeMetadata>();
		std::vector<code::TypeOfData>             types;  /// used only for error messages
		code::builders::TypesContext<>            types_context_adding;
	};

	class Loader final {
	private:
		bool validate_program;

	public:
		Loader(bool validate_program);

		/**
		 * @brief Parses the file, returns the representation of the program with type metadata.
		 *
		 * @param file
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, LoaderLogger> getProgram(const fs::FilePath& file);

		/**
		 * @brief Parses a list of files, returns the representation of the program with type
		 * metadata.
		 *
		 * @param files
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, LoaderLogger>
			getProgram(const std::vector<fs::FilePath>& files);

		std::expected<low::LowVMProgram, LoaderLogger> getProgram(
			const std::vector<code::Function>& functions, const std::vector<code::TypeOfData>& types
		);
	};
}
