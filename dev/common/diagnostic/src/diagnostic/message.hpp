/**
 * @file message.hpp
 * @brief This file describes the abstractions and interface of diagnostic messages in the compiler.
 *
 * All classes in this file are abstract, thus uninstantiable.
 * Any concrete Message subclasses should extend the Error, Warning, or Info abstract classes.
 */

#pragma once

#include <printer/message.hpp>
#include <base/optional.hpp>

#include "source_position.hpp"

namespace dia {
	class Note;

	/**
	 * @brief Abstract base class for storing diagnostic messages generated during the compilation
	 * process.
	 *
	 * Not to be confused with printer::Message.
	 *
	 * Each dia::Message can be converted to a printer::MessagePack via the method toMessagePack.
	 *
	 * A Message can be either an Error, Warning, or Info. It must not be extended with
	 * the exception of these three cases.
	 *
	 * A Message must be supplied with a SourcePosition. It may be supplied with a cause. It may
	 * be supplied with an arbitrary number of Notes, which provide additional, helpful information.
	 */
	class Message {
		SourcePosition                         source_position;
		base::Optional<base::unique_ptr<Note>> cause{};
		std::vector<base::unique_ptr<Note>>    notes{};

	public:
		/**
		 * @brief The severity of the message.
		 *
		 * This enum should exactly match the direct descendants of the Message class.
		 *
		 * This enum may seem redundant, but together with Message::getSeverity, it is a convenient
		 * way to type-match.
		 */
		enum class Severity { Error, Warning, Info };

	private:
		/**
		 * @brief Converts a message severity to a printable MessageContent.
		 * @param s The severity of the message.
		 * @return The stringified severity. All outputs are of equal length.
		 */
		[[nodiscard]]
		static printer::MessageContent severityToMessageContent(Severity s);

		/**
		 * @brief Converts a message severity to MessageType.
		 * @param s The severity of the message.
		 * @return The corresponding MessageType.
		 */
		[[nodiscard]]
		static printer::MessageType severityToMessageType(Severity s);

	public:
		/**
		 * @brief The domain, or thematic focus of the error.
		 *
		 * This may help the user to visually parse the printed messages, as well as enable the user
		 * to filter see only messages in a specific domain for a more focused debugging experience.
		 *
		 * There are no requirements put upon the set of domains, other than to use common sense.
		 *
		 * The domains for all message types (Error, Warning, and Info) are defined in the top-level
		 * Message class, so as to make it easier to guarantee consistency in implementation.
		 */
		enum class Domain {
			/* Error domains */
			Lexer,                 ///< For errors in the lexer.
			Parser,                ///< For errors in the parser.
			Lookup,                ///< For errors in lookup.
			TypeCheck,             ///< For errors when type checking.
			CompileTimeExecution,  ///< For errors resulting from compile time code execution.
			SafetyViolation,       ///< For errors resulting from violation of visibility rules,
			                       ///< immutability rules, reference uniqueness rules, etc.

			/* Warning domains */
			Unused,     ///< For warnings about dead, unused code, like unused variables.
			Unoptimal,  ///< For warnings about code that can be optimised or shortened.
			TooLax,     ///< For warnings about missing annotations improving safety,
			            ///< e.g. missing "final" in class declaration, or unused
			            ///< annotations decreasing safety, e.g. var instead of let
			            ///< in a variable declaration that is not mutated.

			/* Info domains */
			// Currently none

			/* Miscellaneous */
			Misc,  ///< For everything else.
		};

	private:
		/**
		 * @brief Converts a message domain to a printable MessageContent.
		 * @param d The domain of the message.
		 * @return The stringified domain.
		 */
		[[nodiscard]]
		static printer::MessageContent domainToMessageContent(Domain d);

	protected:
		/**
		 * @brief Get a printer::MessageContent ready to be printed for the user to view.
		 *
		 * Note: the MessageContent **must not** include the severity, domain, or notes.
		 *
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the message and
		 * may be used to explain the message to beginners.
		 * @return A printer::MessageContent ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual printer::MessageContent getBaseMessageContent(bool detailed) const
			= 0;

		/**
		 * @brief Construct a Message from a relevant SourcePosition.
		 * @param source_position The SourcePosition relevant to this Message.
		 */
		explicit Message(const SourcePosition& source_position): source_position(source_position) {}

		/**
		 * @brief Construct a Message from a relevant SourcePosition and cause.
		 * @param source_position The SourcePosition relevant to this Message.
		 * @param cause The cause of this Message, which is another Message.
		 */
		explicit Message(const SourcePosition& source_position, base::unique_ptr<Message> cause):
			  source_position(source_position),
			  cause(cause.release()) {}

	public:
		/**
		 * @brief Get a printer::MessagePack ready to be printed for the user to view.
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the error and may be
		 * used to explain the error to beginners.
		 * @return A printer::MessagePack ready to be printed for the user.
		 */
		[[nodiscard]]
		printer::MessagePack toPrinterMessagePack(bool detailed) const;

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
		 * @brief Add a Note to this Message.
		 */
		void addNote(base::unique_ptr<Note> note) { notes.emplace_back(note.release()); }

		virtual ~Message() noexcept = default;
	};

	/**
	 * @brief Abstract base class for storing errors generated during the compilation process.
	 *
	 * Each Error can be converted to a printer::MessagePack via the method toMessagePack.
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

		/**
		 * @brief Construct an Error from a relevant SourcePosition and cause.
		 *
		 * @copydetails Message::Message(const SourcePosition&, base::unique_ptr<Message>)
		 */
		Error(const SourcePosition& source_position, base::unique_ptr<Message> cause):
			  Message(source_position, base::unique_ptr{cause.release()}) {}
	};

	/**
	 * @brief Abstract base class for storing warnings generated during the compilation process.
	 *
	 * Each Warning can be converted to a printer::MessagePack via the method toMessagePack.
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

		/**
		 * @brief Construct a Warning from a relevant SourcePosition and cause.
		 *
		 * @copydetails Message::Message(const SourcePosition&, base::unique_ptr<Message>)
		 */
		Warning(const SourcePosition& source_position, base::unique_ptr<Message> cause):
			  Message(source_position, base::unique_ptr{cause.release()}) {}
	};

	/**
	 * @brief Abstract base class for storing auxiliary diagnostic information generated during the
	 * compilation process.
	 *
	 * Each Info can be converted to a printer::MessagePack via the method toMessagePack.
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

		/**
		 * @brief Construct an Info from a relevant SourcePosition and cause.
		 *
		 * @copydetails Message::Message(const SourcePosition&, base::unique_ptr<Message>)
		 */
		Info(const SourcePosition& source_position, base::unique_ptr<Message> cause):
			  Message(source_position, base::unique_ptr{cause.release()}) {}
	};

	/**
	 * @brief A supplementary piece of information aimed to enhance a dia::Message.
	 *
	 * Examples of a Note include "note: previous declaration here" in a redeclaration message.
	 *
	 * Each Note has two (not necessarily different) printable messages. One brief,
	 * and one detailed. The latter may contain extra information about the source
	 * or nature of the message and may be used to explain the message to beginners.
	 */
	class Note {
	protected:
		/**
		 * @brief Convert the Note to a printer::Message containing the diagnostic minimum.
		 * @return The brief printer::Message ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual printer::MessageContent toMessageContentBrief()
			= 0;

		/**
		 * @brief Convert the Note to a printer::Message which possibly contains information
		 * not included in the diagnostic minimum, thus not included in the brief message.
		 * @return The detailed printer::Message ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual printer::MessageContent toMessageContentDetailed() {
			// By default, the detailed version is the same as the brief version.
			return toMessageContentBrief();
		}

	public:
		/**
		 * @brief Convert the Note to a printer::Message.
		 *
		 * Typically, this takes the content of the note, then decorates it with a coloured "NOTE"
		 * prefix.
		 *
		 * This method is virtual, because one may want to add other decorations, like a location.
		 * Unlike a Message, a Note is not required to contain a SourcePosition.
		 *
		 * @param detailed Whether to include more details than the diagnostic minimum.
		 * @return The printer::Message ready to be printed for the user.
		 */
		[[nodiscard]]
		virtual printer::Message toPrinterMessage(bool detailed);

		virtual ~Note() noexcept = default;
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
		 * @copydoc Note
		 *
		 * This override adds a source location decoration to the printed message.
		 */
		// @FIXME: unimplemented. Pending decision on how to split responsibility between
		// this method and SourcePosition::genErrorStr.
		[[nodiscard]]
		printer::Message toPrinterMessage(bool detailed) override
			= 0;
	};
}
