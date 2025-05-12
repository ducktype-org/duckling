#pragma once

#include <diagnostic/logger.hpp>

#include <base/variant.hpp>

#include <vm/bytecode/element_base.hpp>

#include <type_traits>

namespace vm::loader {
	template<class T>
	concept IsElementVariant = requires(T t) {
		std ::visit([](auto&& elem) { return static_cast<const code::ElementBase&>(elem); }, t);
	};

	/**
	 * @brief Logger that meets loader's requirements for optional source positions.
	 */
	class LoaderLogger final {
		dia::Logger              logger;
		std::vector<std::string> errors;


	public:
		LoaderLogger() {
			this->clear();
		};

		LoaderLogger(dia::Logger&& logger): logger(std::move(logger)) {}

		LoaderLogger(const LoaderLogger&)            = delete;
		LoaderLogger& operator=(const LoaderLogger&) = delete;
		LoaderLogger(LoaderLogger&&)                 = default;
		LoaderLogger& operator=(LoaderLogger&&)      = default;

		template<class ErrT, class Function, class... Args>
		void logMap(const IsElementVariant auto& elem, const Function& callback, Args&&... args) {
			logMap<ErrT>(
				VISIT(elem, e, return static_cast<const code::ElementBase&>(e)),
				callback,
				std::forward<Args>(args)...
			);
		}

		/**
		 * @brief A more specialized version of error logging than `log`. It allows for attaching
		 * notes to error messages by calling a callback with an error message as parameter.
		 */
		template<class ErrT, class Function, class... Args>
		requires std::is_base_of_v<dia::Error, ErrT>
		void logMap(const code::ElementBase& elem, const Function& callback, Args&&... args) {
			match_optional(elem.bytecode_pos) {
				opt_none errors.push_back(base::strConcat(ErrT::ERR_MSG, std::forward<Args>(args)...)
				);
				opt_some(pos) {
					auto t = makeBox<ErrT>(pos, std::forward<Args>(args)...);
					callback(t);
					logger.log(std::move(t));
				}
			}
		}

		/**
		 * @brief Helper to add notes to error messages based on availability of bytecode source
		 * position.
		 */
		template<class NoteT, class ErrT, class... Args>
		requires std::is_base_of_v<dia::Note, NoteT>
		void addNote(Box<ErrT>& error, const code::ElementBase& elem, Args&&... args) {
			match_optional(elem.bytecode_pos) {
				opt_none errors.push_back(
					base::strConcat(NoteT::ERR_MSG, std::forward<Args>(args)...)
				);
				opt_some(pos) { error->addNote(makeBox<NoteT>(pos, std::forward<Args>(args)...)); }
			}
		}

		template<class NoteT, class ErrT, class... Args>
		requires std::is_base_of_v<dia::Note, NoteT>
		void addNote(Box<ErrT>& error, const IsElementVariant auto& elem, Args&&... args) {
			addNote<NoteT>(
				error,
				VISIT(elem, e, return static_cast<const code::ElementBase&>(e)),
				std::forward<Args>(args)...
			);
		}

		/**
		 * @brief The main method for logging errors. If given element has a bytecode source
		 * position, then a proper error will be logged, otherwise error's message will be appended
		 * to internal list or error messages.
		 */
		template<class ErrT, class... Args>
		requires std::is_base_of_v<dia::Error, ErrT>
		void log(const code::ElementBase& elem, Args&&... args) {
			logMap<ErrT>(elem, [](const Box<ErrT>&) {}, std::forward<Args>(args)...);
		}

		template<class ErrT, class... Args>
		requires std::is_base_of_v<dia::Error, ErrT>
		void log(const IsElementVariant auto& elem, Args&&... args) {
			logMap<ErrT>(elem, [](const Box<ErrT>&) {}, std::forward<Args>(args)...);
		}

		/**
		 * @brief Simple error where no source position is available (ex. no main function in the
		 * file).
		 */
		void logSimple(std::string err) { errors.emplace_back(std::move(err)); }

		void dump(std::ostream& stream) const {
			if (logger.bad()) {
				logger.dumpLog(true, stream);
				stream << '\n';
			}
			for (auto& msg: errors) stream << msg << '\n';
		}

		void clear() {
			logger.clear();
			errors.clear();
		}

		bool good() { return logger.good() && errors.empty(); }

		bool bad() { return !good(); }
	};
}
