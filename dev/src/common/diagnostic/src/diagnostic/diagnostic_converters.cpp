#include "diagnostic_converters.hpp"

#include <token_source/source.hpp>

#include <iomanip>
#include <sstream>

namespace dia {
	/**
	 * @brief Converts a message severity to a printable PrinterContent.
	 * @param s The severity of the message.
	 * @return The stringified severity. All outputs are of equal length.
	 */
	[[nodiscard]]
	printer::PrinterContent severityToPrinterContent(const Message::Severity s) {
		using enum Message::Severity;
		switch (s) {
		case Error:
			return { " ERR", printer::Color::BRIGHT_RED };
		case Warning:
			return { "WARN", printer::Color::BRIGHT_MAGENTA };
		case Info:
			return { "INFO", printer::Color::BRIGHT_BLUE };
		case Hint:
			return { "HINT", printer::Color::BRIGHT_CYAN };
		default:
			CORE_PANIC("Unknown message severity.");
		}
	}

	/**
	 * @brief Converts a message domain to a printable std::string.
	 * @param d The domain of the message.
	 * @return The stringified domain.
	 */
	[[nodiscard]]
	std::string domainToString(const Message::Domain d) {
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
		case StaticVerification:
			return "Static verification";
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
			CORE_PANIC("Unknown message domain.");
		}
	}

	printer::PrinterContentsSeq DiagnosticToUserConverter::toPrinterContents(
		CRef<Message> message_ptr, bool detailed
	) {
		// Prepare the leading message.
		const Message::Severity s = message_ptr->getSeverity();
		const Message::Domain   d = message_ptr->getDomain();

		const auto severity_tag = severityToPrinterContent(s);
		const auto domain_tag   = domainToString(d);

		auto result_contents = message_ptr->getSourcePosition().genPrinterContents(
			{ message_ptr->toString(detailed) }
		);
		result_contents.insert(result_contents.begin(), { severity_tag, " [", domain_tag, "]:\n" });

		// Append the notes.
		for (const auto& note_ptr: message_ptr->getNotes()) {
			result_contents.emplace_back("\n");
			for (auto& note_content: toPrinterContents(note_ptr.ref(), message_ptr, detailed))
				result_contents.emplace_back(std::move(note_content));
		}

		return result_contents;
	}

	printer::PrinterContentsSeq DiagnosticToUserConverter::toPrinterContents(
		CRef<Note> note_ptr, CRef<Message>, bool detailed
	) {
		const printer::PrinterContent severity_tag = { "NOTE", printer::Color::BRIGHT_GREEN };

		auto result_contents
			= note_ptr->getSourcePosition()
		          .map([&](const SourcePosition& pos) {
					  return pos.genPrinterContents({ note_ptr->toString(detailed) });
				  })
		          .valueOr({ note_ptr->toString(detailed) });
		result_contents.insert(result_contents.begin(), { severity_tag, "\n" });

		return result_contents;
	}

	namespace {
		/**
		 * @brief Converts a SourcePosition to a Range JSON according to the specification here:
		 * https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#range
		 * @param sp The SourcePosition to convert.
		 * @return A PrinterContent sequence with a JSON representation of the SourcePosition.
		 */
		printer::PrinterContentsSeq sourcePositionToLspJson(SourcePosition sp) {
			auto [start_line, start_col] = sp.getStartLineColumn();
			auto [end_line, end_col]     = sp.getEndLineColumn();
			// clang-format off
			return {
				"{\n",
					"\"start\":", "{\n",
						"\"line\":", std::to_string(start_line), ",\n",
						"\"character\":", std::to_string(start_col), "\n",
					"},\n",
					"\"end\":", "{\n",
						"\"line\":", std::to_string(end_line), ",\n",
						"\"character\":", std::to_string(end_col), "\n",
					"}\n",
				"}",
			};
			// clang-format on
		}

		/**
		 * @brief Converts a Message::Severity to a JSON according to the specification here:
		 * https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticSeverity
		 * @param s The Message::Severity to convert.
		 * @return A PrinterContent sequence with a JSON representation of the Message::Severity.
		 */
		printer::PrinterContent severityToLspJson(Message::Severity s) {
			using enum Message::Severity;
			switch (s) {
			case Error:
				return "1";
			case Warning:
				return "2";
			case Info:
				return "3";
			case Hint:
				return "4";
			default:
				CORE_PANIC("Unknown message severity.");
			}
		}

		printer::PrinterContentsSeq notesToLspJson(
			const std::vector<Box<Note>>& notes, CRef<Message> parent_message, const bool detailed
		) {
			printer::PrinterContentsSeq result{ "[" };

			bool prepend_comma = false;
			for (const auto& note: notes) {
				if (prepend_comma) result.emplace_back(",");
				result.emplace_back("\n");

				auto note_printer_contents = DiagnosticToJSONConverter::toPrinterContents(
					note.ref(), parent_message, detailed
				);
				result.insert(
					result.end(), note_printer_contents.begin(), note_printer_contents.end()
				);

				prepend_comma = true;
			}

			result.insert(result.end(), { "\n", "]" });
			return result;
		}
	}

	// Code adapted from Stack Overflow:
	// https://stackoverflow.com/a/33799784/14406682
	// @TODO: Switch this and the other JSON converter methods out to use a reliable JSON library,
	//  if / when we decide to use nlohmann/json instead of whatever we're using now.
	std::string escapeJson(const std::string& s) {
		std::ostringstream o;
		o << "\"";
		for (char c: s)
			if (c == '"' || c == '\\' || ('\x00' <= c && c <= '\x1f'))
				o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
			else
				o << c;
		o << "\"";
		return o.str();
	}

	printer::PrinterContentsSeq DiagnosticToJSONConverter::toPrinterContents(
		CRef<Message> message_ptr, bool detailed
	) {
		// Prepare complex subJSONs.
		auto range = sourcePositionToLspJson(message_ptr->getSourcePosition());
		auto notes = notesToLspJson(message_ptr->getNotes(), message_ptr, detailed);

		// `data` contains both versions of the message, brief and detailed.
		// clang-format off
		auto data = printer::PrinterContentsSeq{
			"{\n",
				"\"messageBrief\":",    escapeJson(message_ptr->toString(false)), ",\n",
				"\"messageDetailed\":", escapeJson(message_ptr->toString(true)),  "\n",
			"}"
		};
		// clang-format on

		// Prepare main JSON.
		// clang-format off
		printer::PrinterContentsSeq result = {
			"{\n",
				"\"range\":", /* insert at index: 2 */ ",\n",
				"\"severity\":", severityToLspJson(message_ptr->getSeverity()), ",\n",
				// `code` not yet supported.
				// `codeDescription` not yet supported.
				"\"source\":", "\"", domainToString(message_ptr->getDomain()), "\",\n",
				"\"message\":", escapeJson(message_ptr->toString(detailed)), ",\n",
				// `tags` not yet supported.
				"\"relatedInformation\":", /* insert at index: 14 */ ",\n",
				"\"data\":", /* insert at index: 16 */ "\n",
			"}",
		};
		// clang-format on

		// Inject subJSONs (in reverse order so as not to mess with indices).
		result.insert(result.begin() + 16, data.begin(), data.end());
		result.insert(result.begin() + 14, notes.begin(), notes.end());
		result.insert(result.begin() + 2, range.begin(), range.end());

		return result;
	}

	printer::PrinterContentsSeq DiagnosticToJSONConverter::toPrinterContents(
		CRef<Note> note_ptr, CRef<Message> parent_message, bool detailed
	) {
		auto source_position
			= note_ptr->getSourcePosition().valueOr(parent_message->getSourcePosition());
		auto source_uri = source_position.getSource()->getPath().uri();
		auto range      = sourcePositionToLspJson(source_position);

		// clang-format off
		auto result = printer::PrinterContentsSeq {
			"{\n",
				"location:", "{\n",
					"uri:", escapeJson(source_uri), ",\n",
					"range:", /* insert at index 7 */ "\n",
				"}\n",
				"message:", escapeJson(note_ptr->toString(detailed)), "\n",
			"}",
		};
		// clang-format on

		result.insert(result.begin() + 7, range.begin(), range.end());

		return result;
	}
}
