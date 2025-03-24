#pragma once

#include "base/exceptions.hpp"
#include "diagnostic/logger.hpp"
#include "vm/preprocessor/parser/elements.hpp"
#include "vm/program/program.hpp"
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>
#include <base/for_each.hpp>

namespace vm {
#define OPARG(oparg) const opargs::oparg*,
	using PosMap = base::HashMap<
		std::variant<
			FOR_EACH(OPARG, VM_OPARG_TYPES) const vm::Type*,
			const program::Function*,
			const program::Instruction*,
			const vm::program::TypeOfData*>,
		dia::SourcePosition>;
#undef OPARG

	using PreprocessorErrorTp = std::variant<dia::Logger, std::string>;

	using OptPosMapCRef = base::Optional<const PosMap&>;

	struct PreprocessorLogger {
	private:
		dia::Logger              logger;
		std::vector<std::string> errors;
		OptPosMapCRef            pos_map;

	public:
		PreprocessorLogger(OptPosMapCRef pos_map): pos_map(pos_map) {}

		PreprocessorLogger(dia::Logger&& logger, OptPosMapCRef pos_map):
			  logger(std::move(logger)),
			  pos_map(pos_map) {}

		PreprocessorLogger(dia::Logger&& logger): logger(std::move(logger)) {}

		PreprocessorLogger(const PreprocessorLogger&)            = delete;
		PreprocessorLogger(PreprocessorLogger&&)                 = default;
		PreprocessorLogger& operator=(const PreprocessorLogger&) = delete;
		PreprocessorLogger& operator=(PreprocessorLogger&&)      = default;

		template<class T, class Elem, class... Args>
		void log(const Elem* elem, Args&&... args) {
			match_optional(pos_map) {
				opt_none errors.push_back(base::strConcat(T::ERR_MSG, " ", args...));
				opt_some(map) {
					auto t = makeBox<T>(map.at(elem), std::forward<Args>(args)...);
					logger.log(std::move(t));
				}
			}
			CORE_UNREACHABLE();
		}

		template<class T, class Elem, class Function, class... Args>
		void logMap(const Elem* elem, const Function& function, Args&&... args) {
			match_optional(pos_map) {
				opt_none errors.push_back(base::strConcat(T::ERR_MSG, " ", args...));
				opt_some(map) {
					auto t = makeBox<T>(map.at(elem), std::forward<Args>(args)...);
					function(t.ref(), map);
					logger.log(std::move(t));
				}
			}
			CORE_UNREACHABLE();
		}

		bool good() { return logger.good() && errors.empty(); }
	};

	class Preprocessor {
	private:
		bool validate_program;

		// Without pos map
		std::expected<low::LowVMProgram, PreprocessorLogger>
			getProgram(const std::vector<program::CodeFile>& code_files, OptPosMapCRef pos_map);

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
