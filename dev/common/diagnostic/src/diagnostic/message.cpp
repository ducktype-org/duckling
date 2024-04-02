#include "message.hpp"

namespace dia {
	// @FIXME: The " ERR", "WARN", "INFO" raw strings are inconsistent with the style in
	// SourcePosition::genErrorMsg.
	printer::MessageContent Message::severityToMessageContent(const Severity s) {
		using enum Severity;
		switch (s) {
		case Error:
			return printer::MessageContent(" ERR", printer::Color::BRIGHT_RED);
		case Warning:
			return printer::MessageContent("WARN", printer::Color::BRIGHT_MAGENTA);
		case Info:
			return printer::MessageContent("INFO", printer::Color::BRIGHT_BLUE);
		default:
			RIFT_PANIC("Unknown message severity.");
		}
	}

	printer::MessageType Message::severityToMessageType(const Severity s) {
		using enum Severity;
		switch (s) {
		case Error:
			return printer::MessageType::ERROR;
		case Warning:
			return printer::MessageType::WARNING;
		case Info:
			return printer::MessageType::DEBUG;
		default:
			RIFT_PANIC("Unknown message severity.");
		}
	}

	printer::MessageContent Message::domainToMessageContent(const Domain d) {
		using enum Domain;
		switch (d) {
		case Lexer:
			return "Lexer";
		case Parser:
			return "Parser";
		case Lookup:
			return "Lookup";
		case TypeCheck:
			return "Type checking";
		case CompileTimeExecution:
			return "Compile time execution";
		case SafetyViolation:
			return "Safety violation";
		case Unused:
			return "Unused code";
		case Unoptimal:
			return "Unoptimal code";
		case TooLax:
			return "Lax annotations";
		case Misc:
			return "Miscellaneous";
		default:
			RIFT_PANIC("Unknown message severity.");
		}
	}

	printer::MessagePack Message::toPrinterMessagePack(const bool detailed) const {
		Severity s = getSeverity();
		Domain   d = getDomain();

		printer::MessagePack messages;
	}

	// @FIXME: The "NOTE" raw string is inconsistent with the style in SourcePosition::genErrorMsg.
	printer::Message Note::toPrinterMessage(const bool detailed) {
		// Take the content of the note, then decorate it with a coloured "NOTE" prefix.
		printer::MessageContent content = [=] {
			if (detailed) return toMessageContentDetailed();
			return toMessageContentBrief();
		}();
		return printer::Message(
			{ printer::MessageContent{ "NOTE", printer::Color::BRIGHT_CYAN }, ":\n", content },
			printer::MessageType::NOTE
		);
	}
}
