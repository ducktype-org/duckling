/**
 * @file message.hpp
 * @brief This file describes the abstractions and interface of diagnostic messages in the compiler.
 *
 * All classes in this file are abstract, thus uninstantiable.
 * Any concrete Message subclasses should extend the Error, Warning, Info, or Hint abstract classes.
 * They should be defined as locally as possible.
 *
 * ### Usage:
 @code
 // Extend Error, Warning, or Info.
    class MyMessage: public Error {
        // ...

    protected:
        dia::Message::Domain getDomain() const override { ... }

        printer::PrinterContent toStringBrief() const override {
            // ...
        }
    };
 @endcode
 *
 * Interface
 * =========
 *
 * All symbols are in namespace `dia`.
 *
 * Message class
 * -------------
 *
 * The `Message` is the base class for all compiler-generated messages.
 *
 * The compiler reports errors via the `dia::Logger`, which consumes `Message` objects.
 *
 * Each `Message` must be supplied with a `dia::SourcePosition`.
 *
 * The `Message` class is abstract, but one should not derive from it directly. Instead, one should
 always
 * inherit from the `Error`, `Warning`, `Info`, or `Hint` abstract classes. Then, two or three
 methods need to be implemented.
 *
 * First, is the `getDomain()` method. It's straightforward. Simply indicate which
 `dia::Message::Domain` the message
 * pertains to. If no domain suits your needs and it is reasonable to add a new domain, feel free to
 do so.
 *
 * Second, is the `toStringBrief()` method. It describes the cause of the message as briefly as
 possible, while
 * providing the user with enough information to eliminate the error, e.g. "Redeclaration of symbol
 <symbol_name>."
 *
 * Third, is the optionally overridable `toStringDetailed()` method. It behaves similarly to the
 `toStringBrief()`
 * method, but attempts to give more context, for example information useful to beginners, or
 examples of when the message may be thrown.
 * By default, it is implemented to return the exact same information as its brief counterpart.
 *
 * Be careful not to override the (non-virtual) `toString(bool)` method.
 *
 * ### addNote
 *
 * The `Message` class can also be supplied with instances of `dia::Note` via the `addNote(Note)`
 method.
 *
 * Note class
 * ----------
 *
 * A `Note` is meant to supplement a `Message` with an additional helpful piece of information for
 the user.
 * For example, it could supplement a redeclaration error with "Previous declaration here."
 *
 * Just like a `Message`, a `Note` sub-class must be implemented for each case.
 *
 * Unlike a `Message`, a `Note` need not be supplied with a `SourcePosition`. However, since it
 often will
 * be, an additional `NoteWithPosition` abstract class is available.
 */

#pragma once

#include "source_position.hpp"

#include <base/pointers/box.hpp>

#include <printer/printer_content.hpp>

#include <concepts>

namespace dia {
	/**
	 * @brief A supplementary piece of information aimed to enhance a dia::Message.
	 *
	 * Examples of a Note include "note: previous declaration here" in a redeclaration message.
	 *
	 * Each Note can be converted to a printer::printerContentsSeq via the method
	 * toPrinterContents of a DiagnosticToPrinterConverter.
	 *
	 * Each Note has two (not necessarily different) printable messages. One brief,
	 * and one detailed. The latter may contain extra information about the source
	 * or nature of the message and may be used to explain the message to beginners.
	 */
	class Note {
	protected:
		/**
		 * @brief Convert the Note to an std::string containing the diagnostic minimum.
		 * @return The brief std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual std::string toStringBrief() const
			= 0;

		/**
		 * @brief Convert the Note to an std::string which possibly contains information
		 * not included in the diagnostic minimum, thus not included in the brief message.
		 * @return The detailed std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual std::string toStringDetailed() const {
			// By default, the detailed version is the same as the brief version.
			return toStringBrief();
		}

	public:
		/**
		 * @brief Get the SourcePosition relevant to this Note, if it exists.
		 * @return The SourcePosition relevant to this Note.
		 */
		[[nodiscard]]
		virtual base::Optional<SourcePosition> getSourcePosition() const {
			return {};
		}

		/**
		 * @brief Get an std::string ready to be printed for the user to view.
		 *
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the message and
		 * may be used to explain the message to beginners.
		 * @return An std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		std::string toString(bool detailed) const {
			return detailed ? toStringBrief() : toStringDetailed();
		}

		virtual ~Note() noexcept = default;
	};

	/**
	 * @brief Abstract base class for storing diagnostic messages generated during the compilation
	 * process.
	 *
	 * Each dia::Message can be converted to a printer::printerContentsSeq via the method
	 * toPrinterContents of a DiagnosticToPrinterConverter.
	 *
	 * A Message can be either an Error, Warning, Info, or Hint. It must not be extended with
	 * the exception of these four cases.
	 *
	 * A Message must be supplied with a SourcePosition. It may be supplied with
	 * an arbitrary number of Notes, which provide additional, helpful information.
	 */
	class Message {
		SourcePosition         source_position;
		std::vector<Box<Note>> notes{};
		// @FIXME: should we include a `cause` field here?
		// Do we expect to detect when an error is caused by another error?

	public:
		/**
		 * @brief The severity of the message.
		 *
		 * This enum should exactly match the direct descendants of the Message class.
		 *
		 * This enum may seem redundant, but together with Message::getSeverity, it is a convenient
		 * way to type-match.
		 */
		enum class Severity { Error, Warning, Info, Hint };

		/**
		 * @brief The number of different Severity options.
		 */
		static constexpr int NUM_SEVERITIES = 4;

		/**
		 * @brief The domain, or thematic focus of the error.
		 *
		 * This may help the user to visually parse the printed messages, as well as enable the user
		 * to filter see only messages in a specific domain for a more focused debugging experience.
		 *
		 * There are no requirements put upon the set of domains, other than to use common sense.
		 *
		 * The domains for all message types (Error, Warning, Info, and Hint)
		 * are defined in the top-level Message class, so as to make it easier
		 * to guarantee consistency in implementation.
		 */
		enum class Domain {
			/* Error domains */
			Lexer,               ///< For errors in the lexer.
			Parser,              ///< For errors in the parser.
			Lookup,              ///< For errors in lookup.
			TypeCheck,           ///< For errors when type checking.
			StaticVerification,  ///< For errors in static verification which are not type specific.
			CompileTimeExecution,  ///< For errors resulting from compile time code execution.
			SafetyViolation,       ///< For errors resulting from violation of visibility rules,
			                       ///< immutability rules, reference uniqueness rules, etc.

			/* Warning domains */
			Unused,      ///< For warnings about dead, unused code, like unused variables.
			Suboptimal,  ///< For warnings about code that can be optimised or shortened.
			TooLax,      ///< For warnings about missing annotations improving safety,
			             ///< e.g. missing "final" in class declaration, or unused
			             ///< annotations decreasing safety, e.g. var instead of let
			             ///< in a variable declaration that is not mutated.

			/* Info domains */
			// Currently none

			/* Miscellaneous */
			Misc,  ///< For everything else.
		};

	protected:
		/**
		 * @brief Convert the Message to a std::string with the diagnostic minimum.
		 * @return The brief std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual std::string toStringBrief() const
			= 0;

		/**
		 * @brief Convert the Note to an std::string which possibly contains information
		 * not included in the diagnostic minimum, thus not included in the brief message.
		 * @return The detailed std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual std::string toStringDetailed() const {
			// By default, the detailed version is the same as the brief version.
			return toStringBrief();
		}

		/**
		 * @brief Construct a Message from a relevant SourcePosition.
		 * @param source_position The SourcePosition relevant to this Message.
		 */
		explicit Message(const SourcePosition& source_position): source_position(source_position) {}

	public:
		/**
		 * @brief Get am std::string ready to be printed for the user to view.
		 *
		 * Note: the string **must not** include the severity, domain, or notes.
		 *
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the message and
		 * may be used to explain the message to beginners.
		 * @return An std::string ready to be printed for the user.
		 */
		[[nodiscard]]
		std::string toString(const bool detailed) const {
			return detailed ? toStringDetailed() : toStringBrief();
		}

		/**
		 * @brief Get the Severity of the Message.
		 * @return The Severity of the Message.
		 */
		[[nodiscard]]
		virtual Severity getSeverity() const
			= 0;

		/**
		 * @brief Get the Domain of the Message.
		 * @return The Domain of the Message.
		 */
		[[nodiscard]]
		virtual Domain getDomain() const
			= 0;

		/**
		 * @brief Get the SourcePosition relevant to this Message.
		 * @return The SourcePosition relevant to this Message.
		 */
		[[nodiscard]]
		SourcePosition getSourcePosition() const {
			return source_position;
		}

		/**
		 * @brief Get the notes added to this Message.
		 * @return The notes added to this Message.
		 */
		[[nodiscard]]
		const std::vector<Box<Note>>& getNotes() const {
			return notes;
		}

		/**
		 * @brief Add a Note to this Message.
		 *
		 * @param note_ptr A Box to the Note to be added.
		 */
		void addNote(Box<Note> note_ptr) { notes.emplace_back(std::move(note_ptr)); }

		virtual ~Message() noexcept = default;
	};

	/**
	 * @brief Concept for a type which supports converting a Message to a string representation.
	 *
	 * Examples include DiagnosticToUserConverter and DiagnosticToJSONConverter.
	 *
	 * @note This is a concept instead of an abstract base class because the methods are static,
	 * and C++ does not support static virtual methods.
	 */
	template<typename Converter>
	concept DiagnosticToPrinterConverter
		= requires(CRef<Message> message, CRef<Note> note, bool detailed) {
			  {
				  Converter::toPrinterContents(message, detailed)
			  } -> std::same_as<printer::PrinterContentsSeq>;
			  {
				  Converter::toPrinterContents(note, detailed)
			  } -> std::same_as<printer::PrinterContentsSeq>;
			  {
				  Converter::listToPrinterContents(std::vector<CRef<Message>>(), detailed)
			  } -> std::same_as<printer::PrinterContentsSeq>;
		  };

	/**
	 * @brief Abstract base class for storing errors generated during the compilation process.
	 *
	 * Each Error can be converted to a printer::printerContentsSeq via the method
	 * toPrinterContents of a DiagnosticToPrinterConverter.
	 *
	 * An Error must be supplied with a SourcePosition. It may be supplied with an arbitrary
	 * number of Notes, which provide additional, helpful information.
	 */
	class Error: public Message {
		[[nodiscard]]
		Severity getSeverity() const final {
			return Severity::Error;
		}

	protected:
		/**
		 * @brief Construct an Error from a relevant SourcePosition.
		 *
		 * @copydetails Message::Message
		 */
		explicit Error(const SourcePosition& source_position): Message(source_position) {}
	};

	/**
	 * @brief Abstract base class for storing warnings generated during the compilation process.
	 *
	 * Each Warning can be converted to a printer::printerContentsSeq via the method
	 * toPrinterContents of a DiagnosticToPrinterConverter.
	 *
	 * An Warning must be supplied with a SourcePosition. It may be supplied with an arbitrary
	 * number of Notes, which provide additional, helpful information.
	 */
	class Warning: public Message {
		[[nodiscard]]
		Severity getSeverity() const final {
			return Severity::Warning;
		}

	protected:
		/**
		 * @brief Construct a Warning from a relevant SourcePosition.
		 *
		 * @copydetails Message::Message
		 */
		explicit Warning(const SourcePosition& source_position): Message(source_position) {}
	};

	/**
	 * @brief Abstract base class for storing auxiliary diagnostic information generated during the
	 * compilation process.
	 *
	 * Each Info can be converted to a printer::printerContentsSeq via the method
	 * toPrinterContents of a DiagnosticToPrinterConverter.
	 *
	 * An Info must be supplied with a SourcePosition. It may be supplied with an arbitrary
	 * number of Notes, which provide additional, helpful information.
	 */
	class Info: public Message {
		[[nodiscard]]
		Severity getSeverity() const final {
			return Severity::Info;
		}

	protected:
		/**
		 * @brief Construct an Info from a relevant SourcePosition.
		 *
		 * @copydetails Message::Message
		 */
		explicit Info(const SourcePosition& source_position): Message(source_position) {}
	};

	/**
	 * @brief Abstract base class for storing auxiliary diagnostic information generated during the
	 * compilation process.
	 *
	 * Each Hint can be converted to a printer::printerContentsSeq via the method
	 * toPrinterContents of a DiagnosticToPrinterConverter.
	 *
	 * A Hint must be supplied with a SourcePosition. It may be supplied with an arbitrary
	 * number of Notes, which provide additional, helpful information.
	 */
	class Hint: public Message {
		[[nodiscard]]
		Severity getSeverity() const final {
			return Severity::Hint;
		}

	protected:
		/**
		 * @brief Construct a Hint from a relevant SourcePosition.
		 *
		 * @copydetails Message::Message
		 */
		explicit Hint(const SourcePosition& source_position): Message(source_position) {}
	};

	class NoteWithPosition: public Note {
		/**
		 * @brief The source position relevant to this note, e.g. the position of an original
		 * declaration.
		 */
		SourcePosition source_position;

	protected:
		/**
		 * @brief Construct a NoteWithPosition from a relevant SourcePosition.
		 * @param source_position The SourcePosition relevant to this Note.
		 */
		explicit NoteWithPosition(const SourcePosition& source_position):
			  source_position(source_position) {}

	public:
		/**
		 * @copydoc Note::getSourcePosition
		 */
		[[nodiscard]]
		base::Optional<SourcePosition> getSourcePosition() const override {
			return { source_position };
		}
	};

	/***************************
	|   PLACEHOLDER MESSAGES   |
	***************************/

	template<typename BASE_MESSAGE_CLASS>
	concept ValidBaseMessageClass
		= std::same_as<BASE_MESSAGE_CLASS, Error> || std::same_as<BASE_MESSAGE_CLASS, Warning>
	   || std::same_as<BASE_MESSAGE_CLASS, Info> || std::same_as<BASE_MESSAGE_CLASS, Hint>;

	/**
	 * @brief Placeholder message class for when you need to log a message but are
	 * not yet decided on how to implement a proper message class in the given context.
	 *
	 * @deprecated Make your own, specialised message class by inheriting after Error, Warning, or
	 * Info, picking an appropriate name, choosing appropriate data which describe the message and
	 * implementing user-facing message contents.
	 * @TODO #941 #1343 migrate this class to the new DIA 2.0 framework when we
	 * introduce DIA 2.0 in main. We can also consider adding NotYetImplementedMessage, to avoid
	 * throwing.
	 *
	 * @tparam BASE_MESSAGE_CLASS The base class of the message, either Error, Warning, or Info.
	 * @tparam DOMAIN The domain of the message. Pick Message::Domain::Misc if unsure.
	 */
	template<ValidBaseMessageClass BASE_MESSAGE_CLASS, Message::Domain DOMAIN>
	class PlaceholderMessage final: public BASE_MESSAGE_CLASS {
		std::string message;

	public:
		[[nodiscard]]
		Message::Domain getDomain() const override {
			return DOMAIN;
		}

		PlaceholderMessage(const SourcePosition& source_position, std::string message):
			  BASE_MESSAGE_CLASS(source_position),
			  message(std::move(message)) {}

		[[nodiscard]]
		std::string toStringBrief() const override {
			return message;
		}

		static auto make(const SourcePosition& source_position, std::string message) {
			return makeBox<PlaceholderMessage>(source_position, std::move(message));
		}
	};
}
