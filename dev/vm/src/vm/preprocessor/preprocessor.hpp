#pragma once

#include "base/exceptions.hpp"
#include "base/stable_type_id_name_map.hpp"
#include "diagnostic/logger.hpp"
#include "diagnostic/source_position.hpp"
#include "vm/code/builders/builders.hpp"
#include "vm/preprocessor/parser/elements.hpp"
#include "vm/code/code.hpp"
#include "vm/code/type_of_data.hpp"
#include <expected>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>

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
		void log(const code::ElementBase& elem, Args&&... args) {
			match_optional(elem.bytecode_pos) {
				opt_none errors.push_back(base::strConcat(T::ERR_MSG, " ", args...));
				opt_some(pos) {
					auto t = makeBox<T>(pos, std::forward<Args>(args)...);
					logger.log(std::move(t));
				}
			}
		}

		template<class T, class Function, class... Args>
		void logMap(const code::ElementBase& elem, const Function& function, Args&&... args) {
			match_optional(elem.bytecode_pos) {
				opt_none errors.push_back(base::strConcat(T::ERR_MSG, " ", args...));
				opt_some(pos) {
					auto t = makeBox<T>(pos, std::forward<Args>(args)...);
					function(t);
					logger.log(std::move(t));
				}
			}
		}

		void dump(std::ostream& stream) const {
			if (logger.bad()) {
				logger.dumpLog(true, stream);
				stream << '\n';
			}
			for (auto& msg: errors) stream << msg << '\n';
		}

		bool good() { return logger.good() && errors.empty(); }

		bool bad() { return !good(); }
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
			from(const std::vector<vm::code::CodeFile>& code_files);

		const base::StableTypeIdNameMap<code::Function> funcMap() const;
		const TypeMetadata&                             getTypeMetadata() const;

	private:
		Program() = default;

		void insertTypesAndFunctions(
			const std::vector<vm::code::CodeFile>& code_files, vm::PreprocessorLogger& logger
		);
		void defineTypes();

		base::StableTypeIdNameMap<code::Function>   functions;
		base::StableTypeIdNameMap<code::TypeOfData> meta_types;
		TypeMetadata                                types;
	};

	class Preprocessor {
	private:
		bool validate_program;

		std::expected<low::LowVMProgram, PreprocessorLogger> getProgram(
			const std::vector<code::Function>&    functions,
			const code::builders::TypesContext<>& types
		);

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
