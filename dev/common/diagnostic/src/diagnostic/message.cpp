#include "message.hpp"

namespace dia {
	printer::PrinterContent Message::severityToMessageContent(const Severity s) {
		using enum Severity;
		switch (s) {
		case Error:
			return { " ERR", printer::Color::BRIGHT_RED };
		case Warning:
			return { "WARN", printer::Color::BRIGHT_MAGENTA };
		case Info:
			return { "INFO", printer::Color::BRIGHT_BLUE };
		default:
			RIFT_PANIC("Unknown message severity.");
		}
	}

	printer::PrinterContent Message::domainToMessageContent(const Domain d) {
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

	printer::PrinterContent Message::getBaseMessageContent(const bool detailed) const {
		return detailed ? toMessageContentBrief() : toMessageContentDetailed();
	}

	printer::PrinterContentsSeq Message::toPrinterContents(const bool detailed) const {
		// Prepare the leading message.
		const Severity s = getSeverity();
		const Domain   d = getDomain();

		const auto severityTag = severityToMessageContent(s);
		const auto domainTag   = domainToMessageContent(d);

		auto resultContents = source_position.genPrinterContents(getBaseMessageContent(detailed));
		resultContents.insert(resultContents.begin(), { severityTag, " [", domainTag, "]:\n" });

		// Append the notes.
		for (const auto& note: notes) {
			auto noteContents = note->toPrinterContents(detailed);
			for (auto& noteContent: note->toPrinterContents(detailed))
				resultContents.emplace_back(std::move(noteContent));
		}
		// messages.push_back(note->toPrinterMessage(detailed));

		return resultContents;
	}

	// @FIXME: The "NOTE" raw string is inconsistent with the style in SourcePosition::genErrorMsg.
	printer::PrinterContentsSeq Note::toPrinterContents(const bool detailed) {
		// Take the content of the note, then decorate it with a coloured "NOTE" prefix.
		printer::PrinterContent content = [&, detailed] {
			if (detailed) return toMessageContentDetailed();
			return toMessageContentBrief();
		}();
		return { printer::PrinterContent{ "NOTE", printer::Color::BRIGHT_CYAN }, ":\n", content };
	}
}
