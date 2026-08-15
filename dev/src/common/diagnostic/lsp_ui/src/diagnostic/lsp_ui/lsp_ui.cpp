#include "lsp_ui.hpp"

#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/default_deleter.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/core/template_evaluation.hpp>
#include <diagnostic/core/view_constructors.hpp>
#include <filesystem/file_path.hpp>

#include <any>
#include <string>
#include <utility>
#include <vector>

namespace dia::lsp {
	void writeJsonString(std::ostream& out, std::string_view s) {
		out << '"';
		for (char c: s) {
			switch (c) {
			case '"':
				out << "\\\"";
				break;
			case '\\':
				out << "\\\\";
				break;
			case '\n':
				out << "\\n";
				break;
			case '\r':
				out << "\\r";
				break;
			case '\t':
				out << "\\t";
				break;
			default:
				out << c;
			}
		}
		out << '"';
	}

	struct Position {
		u64 line{};
		u64 character{};
	};

	struct Range {
		Position start{};
		Position end{};

		void jsonSerialize(std::ostream& out) const {
			out << "{\n";
			out << R"("start": { "line": )" << start.line << ", \"character\": " << start.character
				<< "},\n";
			out << R"("end": { "line": )" << end.line << ", \"character\": " << end.character
				<< "}\n";
			out << "}";
		}
	};

	struct Location {
		std::string uri;
		Range       range;
	};


	enum class DiagnosticSeverity : u64 { Error = 1, Warning = 2, Information = 3, Hint = 4 };


	enum class DiagnosticTag : u64 { Unnecessary = 1, Deprecated = 2 };

	struct CodeDescription {
		std::string href;
	};

	struct DiagnosticRelatedInformation {
		Location    location;
		std::string message;

		void jsonSerialize(std::ostream& out) const {
			out << "{\n";
			out << R"("location": { "uri": )";
			writeJsonString(out, location.uri);
			out << R"(, "range": )";
			location.range.jsonSerialize(out);
			out << "},\n";
			out << R"("message": )";
			writeJsonString(out, message);
			out << "\n";
			out << "}";
		}
	};

	/**
	 * @brief The diagnostic class is 1-1 mapping of the typescript
	 * Language Server Protocol `Diagnostic` structure.
	 *
	 * The documentation below is taken from the vscode-lsp-extension
	 * nodejs package.
	 */
	class Diagnostic {
	public:
		/**
		 * The range at which the message applies
		 */
		Range range{};

		/**
		 * The diagnostic's severity. Can be omitted. If omitted it is up to the
		 * client to interpret diagnostics as error, warning, info or hint.
		 */
		DiagnosticSeverity severity = DiagnosticSeverity::Information;

		/**
		 * Identifier code of the diagnostic.
		 */
		u64 code{};

		/**
		 * Some extra information about the diagnostic code identifier.
		 */
		base::Optional<CodeDescription> code_description{};

		/**
		 * A human-readable string describing the source of this
		 * diagnostic, e.g. 'typescript' or 'super lint'. It usually
		 * appears in the user interface. In our case, we set it to "Duckling".
		 */
		std::string source{};

		/**
		 * The diagnostic's message. It usually appears in the user interface
		 */
		std::string message{};

		/**
		 * Additional metadata about the diagnostic.
		 *
		 * @since 3.15.0 (lsp protocol)
		 */
		base::Optional<std::vector<DiagnosticTag>> tags{};

		/**
		 * An array of related diagnostic information, e.g. when symbol-names within
		 * a scope collide all definitions can be marked via this property.
		 */
		std::vector<DiagnosticRelatedInformation> related_information{};

		/**
		 * A data entry field that is preserved between a `textDocument/publishDiagnostics`
		 * notification and `textDocument/codeAction` request.
		 *
		 * @since 3.16.0 (lsp protocol)
		 */
		base::Optional<std::any> data{};
	};

	DiagnosticSeverity convertSeverity(dia::term_ui_view::StyleType severity) {
		switch (severity) {
		case dia::term_ui_view::StyleType::Error:
			return DiagnosticSeverity::Error;
		case dia::term_ui_view::StyleType::Warning:
			return DiagnosticSeverity::Warning;
		case dia::term_ui_view::StyleType::Note:
		case dia::term_ui_view::StyleType::Docs:
			return DiagnosticSeverity::Information;
		case dia::term_ui_view::StyleType::Hint:
			return DiagnosticSeverity::Hint;
		}
		return DiagnosticSeverity::Information;  // Default case
	}

	/**
	 * @brief The term ui convention (as well as for example vs code for display)
	 * is to calculate the columns and lines from 1. The end_line and end_col
	 * are inclusive. But the LSP protocol uses 0-based indexing and the end
	 * position is exclusive.
	 */
	Location extractLocation(const dia::term_ui_view::CodeSection& section) {
		Range range;

		range.start.line      = section.location.line - 1;
		range.start.character = section.location.column - 1;

		if (section.location.end_line.has_value() && section.location.end_column.has_value()) {
			range.end.line      = section.location.end_line.value() - 1;
			range.end.character = section.location.end_column.value();
		} else {
			range.end = range.start;
		}
		return Location{ .uri   = fs::FilePath(section.location.file).toPhysicalPath().uri(),
			             .range = range };
	}

	/**
	 * @brief This function serves two purposes:
	 * It's a heuristic approach to extracting the main message location
	 * and also adds "related diagnostic" attachments based on the code sections in the description
	 * (one code section would add one "related diagnostic" attachment).
	 *
	 * @param message The message to extract from.
	 * @param related_information Vector to add related information to.
	 *
	 * @return Range of the main message.
	 */
	Location getMessageLocation(
		const dia::term_ui_view::Message&          message,
		std::vector<DiagnosticRelatedInformation>& related_information,
		const EvaluationContext&                   evaluation_ctx
	) {
		std::string content            = "";
		bool        found_code_section = false;
		Location    loc{};

		for (const auto& section: message.sections) {
			variant_match(section) {
				variant_case(dia::term_ui_view::TextSection, text) { content += text + "\n"; }
				variant_case(dia::term_ui_view::CodeSection, section) {
					if (not found_code_section) {
						loc                = extractLocation(section);
						found_code_section = true;
					} else {
						if (content.empty()) continue;
						Location section_loc = extractLocation(section);

						related_information.push_back(DiagnosticRelatedInformation{
							.location = section_loc, .message = content });
						content = "";
					}
				}
			}
		}
		if (not found_code_section) {
			loc = Location{ .uri   = evaluation_ctx.default_error_location_uri,
				            .range = Range{ .start = Position{ .line = 0, .character = 0 },
				                            .end   = Position{ .line = 0, .character = 0 } } };
		}
		return loc;
	}

	/**
	 * @brief If the evaluation of the diagnostic failed, return a diagnostic
	 * indicating the failure.
	 */
	LSPDiagnosticResult failedResult(
		const std::string& error_msg, const EvaluationContext& evaluation_ctx
	) {
		Box<Diagnostic> diag = makeBox<Diagnostic>();
		diag->range          = Range{ .start = Position{ .line = 0, .character = 0 },
			                          .end   = Position{ .line = 0, .character = 0 } };
		diag->severity       = DiagnosticSeverity::Error;
		diag->code           = 0;
		diag->source         = "Duckling";
		diag->message        = "Failed to evaluate diagnostic: " + error_msg;
		return { std::move(diag), evaluation_ctx.default_error_location_uri };
	}

	LSPDiagnosticResult evaluateToLanguageServerMessage(
		CRef<dia_args::Diagnostic> diagnostic_args, const EvaluationContext& evaluation_ctx
	) {
		term_ui_view::Diagnostic view;
		try {
			auto state = dia::evaluateDiagnostic(*diagnostic_args);
			view       = dia::constructTreeView(state);
		} catch (const std::exception& e) { return failedResult(e.what(), evaluation_ctx); }

		Box<Diagnostic> diag = makeBox<Diagnostic>();
		CORE_ASSERT(
			!view.messages.empty(),
			"Diagnostic view must have at least one message which is the main message."
		);
		const auto& main_msg = view.messages[0];
		Location    loc = getMessageLocation(main_msg, diag->related_information, evaluation_ctx);
		diag->range     = loc.range;
		diag->severity  = convertSeverity(main_msg.type);
		diag->code      = main_msg.code;
		diag->source    = "Duckling";
		diag->message   = main_msg.header;


		for (u64 i = 1; i < view.messages.size(); i++) {
			const auto& msg = view.messages[i];

			// These are code sections from single message (useful if the message has multiple locations)
			std::vector<DiagnosticRelatedInformation> related_info;
			Location msg_loc = getMessageLocation(msg, related_info, evaluation_ctx);
			diag->related_information.push_back(DiagnosticRelatedInformation{
				.location = msg_loc, .message = msg.header });

			diag->related_information.insert(
				diag->related_information.end(),
				std::make_move_iterator(related_info.begin()),
				std::make_move_iterator(related_info.end())
			);
		}

		return { std::move(diag), std::move(loc.uri) };
	}

	void LSPDiagnosticResult::jsonSerializeDiagnostic(
		CRef<Diagnostic> diagnostic, std::ostream& out
	) {
		out << "{\n";
		out << R"("range": )";
		diagnostic->range.jsonSerialize(out);
		out << ",\n";
		out << R"("severity": )" << static_cast<u64>(diagnostic->severity) << ",\n";
		out << R"("code": )" << diagnostic->code << ",\n";
		out << R"("source": )";
		writeJsonString(out, diagnostic->source);
		out << ",\n";
		out << R"("message": )";
		writeJsonString(out, diagnostic->message);

		if (!diagnostic->related_information.empty()) {
			out << ",\n";
			out << R"("relatedInformation": [)" << "\n";
			for (usize i = 0; i < diagnostic->related_information.size(); i++) {
				diagnostic->related_information[i].jsonSerialize(out);
				if (i + 1 < diagnostic->related_information.size()) out << ",\n";
			}
			out << "]\n";
		}
		if (diagnostic->code_description.has_value()) {
			out << ",\n";
			out << R"("codeDescription": { "href": )";
			writeJsonString(out, diagnostic->code_description->href);
			out << " }\n";
		}
		if (diagnostic->tags.has_value()) {
			out << ",\n";
			out << R"("tags": [)";
			for (usize i = 0; i < diagnostic->tags->size(); i++) {
				out << static_cast<u64>((*diagnostic->tags)[i]);
				if (i + 1 < diagnostic->tags->size()) out << ", ";
			}
			out << "]\n";
		}
		if (diagnostic->data.has_value())
			CORE_PANIC("Serialization of 'data' field is not implemented yet.");
		out << "}\n";
	}
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(dia::lsp::Diagnostic);
