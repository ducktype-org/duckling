#include "diagnostic_converters.hpp"
#include "logger.hpp"

namespace dia {
	/**
	 * @brief Converts a message severity to a printable PrinterContent.
	 * @param s The severity of the message.
	 * @return The stringified severity. All outputs are of equal length.
	 */
	[[nodiscard]]
	printer::PrinterContent severityToMessageContent(const Message::Severity s) {
		using enum Message::Severity;
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

	/**
	 * @brief Converts a message domain to a printable PrinterContent.
	 * @param d The domain of the message.
	 * @return The stringified domain.
	 */
	[[nodiscard]]
	printer::PrinterContent domainToMessageContent(const Message::Domain d) {
		using enum Message::Domain;
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
		case Suboptimal:
			return "Suboptimal code";
		case TooLax:
			return "Lax annotations";
		case Misc:
			return "Miscellaneous";
		default:
			RIFT_PANIC("Unknown message severity.");
		}
	}

	printer::PrinterContentsSeq DiagnosticToUserConverter::toPrinterContents(
		base::c_borrow_ptr<Message> message_ptr, bool detailed
	) {
		// Prepare the leading message.
		const Message::Severity s = message_ptr->getSeverity();
		const Message::Domain   d = message_ptr->getDomain();

		const auto severityTag = severityToMessageContent(s);
		const auto domainTag   = domainToMessageContent(d);

		auto resultContents = message_ptr->getSourcePosition().genPrinterContents(
			message_ptr->getPrinterContent(detailed)
		);
		resultContents.insert(resultContents.begin(), { severityTag, " [", domainTag, "]:\n" });

		// Append the notes.
		for (const auto& note: message_ptr->getNotes())
			for (auto& noteContent: toPrinterContents(note.borrow(), detailed))
				resultContents.emplace_back(std::move(noteContent));

		return resultContents;
	}

	printer::PrinterContentsSeq DiagnosticToUserConverter::toPrinterContents(
		base::c_borrow_ptr<Note> note_ptr, bool detailed
	) {
		const printer::PrinterContent severityTag = { "NOTE", printer::Color::BRIGHT_CYAN };

		auto resultContents
			= note_ptr->getSourcePosition()
		          .map([&](SourcePosition pos) {
					  return pos.genPrinterContents(note_ptr->getPrinterContent(detailed));
				  })
		          .value_or({ note_ptr->getPrinterContent(detailed) });
		resultContents.insert(resultContents.begin(), { severityTag, "\n" });

		return resultContents;
	}
}
