#pragma once

#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/placeholder.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <vm/bytecode/element_base.hpp>

namespace vm::loader {
	template<class T>
	concept IsElementVariant = requires(T t) {
		std ::visit([](auto&& elem) { return static_cast<const code::ElementBase&>(elem); }, t);
	};

	/**
	 * @brief Logger that meets loader's requirements for optional source positions.
	 */
	class LoaderLogger final {
		dia_int::Logger          logger;
		std::vector<std::string> errors;


	public:
		LoaderLogger() = default;

		LoaderLogger(dia_int::Logger&& logger): logger(std::move(logger)) {}

		LoaderLogger(const LoaderLogger&)            = delete;
		LoaderLogger& operator=(const LoaderLogger&) = delete;
		LoaderLogger(LoaderLogger&&)                 = default;
		LoaderLogger& operator=(LoaderLogger&&)      = default;

		/**
		 * @brief A more specialized version of error logging than `log`. It allows for attaching
		 * notes to error messages by calling a callback with an error message as parameter.
		 */
		template<class Function>
		void logMap(
			const code::ElementBase&    elem,
			const Function&             callback,
			std::string_view            header_message,
			std::string_view            description             = "",
			base::Optional<std::string> pointer_message_content = "here"
		) {
			match_optional(elem.bytecode_pos) {
				opt_none errors.emplace_back(header_message);
				opt_some(pos) {
					auto t = makeBox<dia_int::PlaceholderError>(
						std::string(header_message),
						pos,
						std::string(description),
						std::move(pointer_message_content)
					);
					callback(t);
					logger.log(std::move(t));
				}
			}
		}

		/**
		 * @brief Helper to add notes to error messages based on availability of bytecode source
		 * position.
		 */
		void addNote(
			Box<dia_int::PlaceholderError>& error,
			const code::ElementBase&        elem,
			std::string_view                header_message,
			std::string_view                description             = "",
			base::Optional<std::string>     pointer_message_content = "here"
		) {
			match_optional(elem.bytecode_pos) {
				opt_none errors.emplace_back(header_message);
				opt_some(pos) {
					error->addAttachedMessage(makeBox<dia_int::PlaceholderNote>(
						std::string(header_message),
						pos,
						std::string(description),
						std::move(pointer_message_content)
					));
				}
			}
		}

		void addNote(
			Box<dia_int::PlaceholderError>& error,
			const IsElementVariant auto&    elem,
			std::string_view                header_message,
			std::string_view                description             = "",
			base::Optional<std::string>     pointer_message_content = "here"
		) {
			addNote(
				error,
				VISIT(elem, e, return static_cast<const code::ElementBase&>(e)),
				header_message,
				description,
				std::move(pointer_message_content)
			);
		}

		/**
		 * @brief The main method for logging errors. If given element has a bytecode source
		 * position, then a proper error will be logged, otherwise error's message will be appended
		 * to internal list or error messages.
		 */
		void log(
			const code::ElementBase&    elem,
			std::string_view            header_message,
			std::string_view            description             = "",
			base::Optional<std::string> pointer_message_content = "here"
		) {
			logMap(
				elem,
				[](const Box<dia_int::PlaceholderError>&) {},
				header_message,
				description,
				std::move(pointer_message_content)
			);
		}

		void logSimple(std::string err) { errors.emplace_back(std::move(err)); }

		void dump(std::ostream& stream) {
			if (logger.bad()) {
				logger.terminalPrint(stream);
				stream << '\n';
			}
			for (auto& msg: errors) stream << msg << '\n';
		}

		bool good() { return logger.good() && errors.empty(); }

		bool bad() { return !good(); }
	};
}
