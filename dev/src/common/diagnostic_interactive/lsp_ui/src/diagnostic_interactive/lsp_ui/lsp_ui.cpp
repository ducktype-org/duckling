#include "lsp_ui.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/default_deleter.hpp>

#include <any>
#include <string>
#include <utility>
#include <vector>

namespace dia_int::lsp {
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

		void serialize(std::ostream& out) const {
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

	using Code = std::string;

	struct CodeDescription {
		std::string href;
	};

	struct DiagnosticRelatedInformation {
		Location    location;
		std::string message;

		void serialize(std::ostream& out) const {
			out << "{\n";
			out << R"("location": { "uri": )";
			writeJsonString(out, location.uri);
			out << R"(, "range": )";
			location.range.serialize(out);
			out << "},\n";
			out << R"("message": )";
			writeJsonString(out, message);
			out << "\n";
			out << "}";
		}
	};

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
		u64 code;

		/**
		 * Some extra information about the diagnostic code identifier.
		 */
		base::Optional<CodeDescription> code_description;

		/**
		 * A human-readable string describing the source of this
		 * diagnostic, e.g. 'typescript' or 'super lint'. It usually
		 * appears in the user interface.
		 */
		std::string source;

		/**
		 * The diagnostic's message. It usually appears in the user interface
		 */
		std::string message;

		/**
		 * Additional metadata about the diagnostic.
		 *
		 * @since 3.15.0
		 */
		base::Optional<std::vector<DiagnosticTag>> tags;

		/**
		 * An array of related diagnostic information, e.g. when symbol-names within
		 * a scope collide all definitions can be marked via this property.
		 */
		std::vector<DiagnosticRelatedInformation> related_information;

		/**
		 * A data entry field that is preserved between a `textDocument/publishDiagnostics`
		 * notification and `textDocument/codeAction` request.
		 *
		 * @since 3.16.0
		 */
		base::Optional<std::any> data;
	};

	DiagnosticSeverity convertSeverity(dia_int::term_ui_view::StyleType severity) {
		switch (severity) {
		case dia_int::term_ui_view::StyleType::Error:
			return DiagnosticSeverity::Error;
		case dia_int::term_ui_view::StyleType::Warning:
			return DiagnosticSeverity::Warning;
		case dia_int::term_ui_view::StyleType::Note:
		case dia_int::term_ui_view::StyleType::Docs:
			return DiagnosticSeverity::Information;
		case dia_int::term_ui_view::StyleType::Hint:
			return DiagnosticSeverity::Hint;
		}
		return DiagnosticSeverity::Information;  // Default case
	}

	Location extractLocation(
		const dia_int::term_ui_view::CodeSection& section, const EvaluationContext& ctx
	) {
		Range range;

		range.start.line      = section.line - 1;
		range.start.character = section.col - 1;

		if (section.end_line.has_value() && section.end_col.has_value()) {
			range.end.line      = section.end_line.value() - 1;
			range.end.character = section.end_col.value();
		} else {
			range.end = range.start;
		}
		return Location{ .uri = ctx.convert_path_to_uri(section.file), .range = range };
	}

	/**
	 * @brief This function serves two purposes:
	 * Is's a heuristic approach to extracting the main message location
	 * and also adds related diagnostic from the code sections in the description
	 * (if there is more than one code section in the msg).
	 *
	 * @param message The message to extract from.
	 * @param related_information Vector to add related information to.
	 *
	 * @return Range of the main message.
	 */
	Location getMessageLocation(
		const dia_int::term_ui_view::Message&      message,
		std::vector<DiagnosticRelatedInformation>& related_information,
		const EvaluationContext&                   ctx
	) {
		std::string content            = "";
		bool        found_code_section = false;
		Location    loc{};

		for (const auto& section: message.sections) {
			variant_match(section) {
				variant_case(dia_int::term_ui_view::TextSection, text) { content += text + "\n"; }
				variant_case(dia_int::term_ui_view::CodeSection, section) {
					if (not found_code_section) {
						loc                = extractLocation(section, ctx);
						found_code_section = true;
					} else {
						if (content.empty()) continue;
						Location section_loc = extractLocation(section, ctx);

						related_information.push_back(DiagnosticRelatedInformation{
							.location = section_loc, .message = content });
					}
				}
			}
		}
		if (not found_code_section) {
			loc = Location{ .uri   = ctx.defaultUri(),
				            .range = Range{ .start = Position{ .line = 0, .character = 0 },
				                            .end   = Position{ .line = 0, .character = 0 } } };
		}
		return loc;
	}

	/**
	 * @brief Fetch the location of the message from the first code section found.
	 * If no code section is found, return a default location.
	 */
	Location getLocationFromMessage(
		const dia_int::term_ui_view::Message& message, const EvaluationContext& ctx
	) {
		for (const auto& section: message.sections) {
			variant_match(section) {
				variant_case(dia_int::term_ui_view::CodeSection, code_section) {
					return extractLocation(code_section, ctx);
				}
				variant_case_novalue(dia_int::term_ui_view::TextSection) {
					// continue searching
				}
			}
		}
		// If no code section found, return a default location
		return Location{ .uri   = ctx.defaultUri(),
			             .range = Range{ .start = Position{ .line = 0, .character = 0 },
			                             .end   = Position{ .line = 0, .character = 0 } } };
	}

	/**
	 * @brief If the evaluation of the diagsnotic failed, return a diagnostic
	 * indicating the failure.
	 */
	LSPDiagnosticResult failedResult(const std::string& error_msg, const EvaluationContext& ctx) {
		Box<Diagnostic> diag = makeBox<Diagnostic>();
		diag->range          = Range{ .start = Position{ .line = 0, .character = 0 },
			                          .end   = Position{ .line = 0, .character = 0 } };
		diag->severity       = DiagnosticSeverity::Error;
		diag->code           = 0;
		diag->source         = "Duckling";
		diag->message        = "Failed to evaluate diagnostic: " + error_msg;
		return { std::move(diag), ctx.defaultUri() };
	}

	LSPDiagnosticResult evaluateToLanguageServerMessage(
		CRef<dia_args::Diagnostic> diagnostic_args, const EvaluationContext& ctx
	) {
		term_ui_view::Diagnostic view;
		try {
			auto state = dia_int::evaluateDiagnostic(*diagnostic_args);
			view       = dia_int::constructTreeView(state);
		} catch (const std::exception& e) { return failedResult(e.what(), ctx); }

		Box<Diagnostic> diag     = makeBox<Diagnostic>();
		const auto&     main_msg = view.messages[0];
		Location        loc      = getMessageLocation(main_msg, diag->related_information, ctx);
		diag->range              = loc.range;
		diag->severity           = convertSeverity(main_msg.type);
		diag->code               = main_msg.code;
		diag->source             = "Duckling";
		diag->message            = main_msg.header;


		for (u64 i = 1; i < view.messages.size(); i++) {
			const auto& msg     = view.messages[i];
			Location    msg_loc = getLocationFromMessage(msg, ctx);
			diag->related_information.push_back(DiagnosticRelatedInformation{
				.location = msg_loc, .message = msg.header });
		}

		return { std::move(diag), std::move(loc.uri) };
	}

	void LSPDiagnosticResult::jsonSerializeDiagnostic(
		CRef<Diagnostic> diagnostic, std::ostream& out
	) {
		out << "{\n";
		out << R"("range": )";
		diagnostic->range.serialize(out);
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
				diagnostic->related_information[i].serialize(out);
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

DEFAULT_BOX_PTR_DELETER_DEFINITION(dia_int::lsp::Diagnostic);
