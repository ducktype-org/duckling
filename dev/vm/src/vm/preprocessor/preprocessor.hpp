#pragma once

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
#include <vm/preprocessor/parser/elements.hpp>

#include <expected>
#include <type_traits>

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

		template<class T, class Function, class... Args>
		requires std::is_base_of_v<dia::Error, T>
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

		template<class T, class... Args>
		requires std::is_base_of_v<dia::Error, T>
		void log(const code::ElementBase& elem, Args&&... args) {
			logMap<T>(elem, [](const Box<T>&) {}, std::forward<Args>(args)...);
		}

		template<class T, class... Args>
		void logSimple(Args&&... args) {
			errors.push_back(base::strConcat(T::ERR_MSG, " ", std::forward<Args>(args)...));
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
		Program(const Program&)            = delete;
		Program(Program&&) noexcept        = default;
		Program& operator=(const Program&) = delete;
		Program& operator=(Program&&)      = default;

		static std::expected<Program, PreprocessorLogger> from(
			const std::vector<code::Function>& functions, const std::vector<code::TypeOfData>& types
		);
		const base::StableTypeIdNameMap<code::Function>& funcMap() const;

		Box<TypeMetadata> produceTypeMetadata() const;

	private:
		Program() = default;

		void insertTypes(const std::vector<code::TypeOfData>& types, PreprocessorLogger& logger);
		void insertFunctions(
			const std::vector<code::Function>& functions, PreprocessorLogger& logger
		);

		base::StableTypeIdNameMap<code::Function> functions;
		Box<TypeMetadata>                         type_metadata = makeBox<TypeMetadata>();
		std::vector<code::TypeOfData>             types;  /// used for logging
		code::builders::TypesContext<>            types_context_adding;
	};

	class Preprocessor {
	private:
		bool validate_program;

		std::expected<low::LowVMProgram, PreprocessorLogger> getProgram(
			const std::vector<code::Function>& functions, const std::vector<code::TypeOfData>& types
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
