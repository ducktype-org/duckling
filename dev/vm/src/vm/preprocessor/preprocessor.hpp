#pragma once

#include "base/exceptions.hpp"
#include "base/stable_type_id_name_map.hpp"
#include "diagnostic/logger.hpp"
#include "diagnostic/source_position.hpp"
#include "vm/preprocessor/parser/elements.hpp"
#include "vm/program/program.hpp"
#include "vm/program/type_of_data.hpp"
#include <expected>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>
#include <base/for_each.hpp>

namespace vm {
	struct PreprocessorLogger {
	private:
		dia::Logger              logger;
		std::vector<std::string> errors;

	public:
		PreprocessorLogger() = default;

		PreprocessorLogger(dia::Logger&& logger): logger(std::move(logger)) {}

		PreprocessorLogger(const PreprocessorLogger&)            = delete;
		PreprocessorLogger(PreprocessorLogger&&)                 = default;
		PreprocessorLogger& operator=(const PreprocessorLogger&) = delete;
		PreprocessorLogger& operator=(PreprocessorLogger&&)      = default;

		template<class T, class... Args>
		void log(const program::ElementBase& elem, Args&&... args) {
			match_optional(elem.bytecode_pos) {
				opt_none errors.push_back(base::strConcat(T::ERR_MSG, " ", args...));
				opt_some(pos) {
					auto t = makeBox<T>(pos, std::forward<Args>(args)...);
					logger.log(std::move(t));
				}
			}
			CORE_UNREACHABLE();
		}

		template<class T, class Function, class... Args>
		void logMap(const program::ElementBase& elem, const Function& function, Args&&... args) {
			match_optional(elem.bytecode_pos) {
				opt_none errors.push_back(base::strConcat(T::ERR_MSG, " ", args...));
				opt_some(pos) {
					auto t = makeBox<T>(pos, std::forward<Args>(args)...);
					function(t);
					logger.log(std::move(t));
				}
			}
			CORE_UNREACHABLE();
		}

		bool good() { return logger.good() && errors.empty(); }
	};

	/**
	 * @brief Preprocessor-only program representation
	 * @todo Improve interface - add access to functions and types not by returning the structures.
	 */
	class Program {
	public:
		Program(const Program&)            = default;
		Program(Program&&) noexcept        = default;
		Program& operator=(const Program&) = default;
		Program& operator=(Program&&)      = default;

		static std::expected<Program, PreprocessorLogger>
			from(const std::vector<vm::program::CodeFile>& code_files);

		const base::StableTypeIdNameMap<program::Function> funcMap() const;
		const TypeMetadata&                                getTypeMetadata() const;

	private:
		Program() = default;

		void insertTypesAndFunctions(
			const std::vector<vm::program::CodeFile>& code_files, vm::PreprocessorLogger& logger
		);
		void defineTypes();

		base::StableTypeIdNameMap<program::Function>   functions;
		base::StableTypeIdNameMap<program::TypeOfData> meta_types;
		TypeMetadata                                   types;
	};

	class Preprocessor {
	private:
		bool validate_program;

		// Without pos map
		std::expected<low::LowVMProgram, PreprocessorLogger>
			getProgram(const std::vector<program::CodeFile>& code_files);

	public:
		Preprocessor(bool validate_program);

		/**
		 * @brief Parses the file, returns the representation of the program with type metadata.
		 *
		 * @param file
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, PreprocessorLogger> getProgram(const fs::FilePath& file);


		/**
		 * @brief Parses a list of files, returns the representation of the program with type
		 * metadata.
		 *
		 * @param files
		 * @return std::expected<vm::LowVMProgram, std::string>
		 */
		std::expected<low::LowVMProgram, PreprocessorLogger>
			getProgram(const std::vector<fs::FilePath>& files);
	};
}
