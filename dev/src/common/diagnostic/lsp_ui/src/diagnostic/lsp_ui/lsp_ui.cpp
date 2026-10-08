// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lsp_ui.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/core/template_evaluation.hpp>
#include <diagnostic/core/view_constructors.hpp>
#include <filesystem/file_path.hpp>

#include <string>
#include <utility>
#include <vector>

namespace dia::lsp {
	using ::lsp::Diagnostic;
	using ::lsp::DiagnosticRelatedInformation;
	using ::lsp::DiagnosticSeverity;
	using ::lsp::Location;
	using ::lsp::Position;
	using ::lsp::Range;
	using ::lsp::Uint;

	/**
	 * @brief A range together with the path of the file it lies in.
	 *
	 * The path only becomes a URI once the language server resolves it, so it is carried
	 * separately until then.
	 */
	struct SectionLocation final {
		std::string file;
		Range       range;
	};

	/**
	 * @brief Normalizes a path a diagnostic carries to the file it names on disk.
	 *
	 * The path arrives as a bare string that has already lost its type, and can be empty, so
	 * neither conversion may be applied blindly.
	 */
	std::string toPhysicalPathString(const std::string& file) {
		if (file.empty()) return {};

		fs::FilePath path(file);
		if (path.isVirtual()) return path.toPhysicalPath().string();
		return path.string();
	}

	/**
	 * @brief A zero-length range at the start of a file, for diagnostics with no location.
	 */
	Range wholeFileStart() {
		return { .start = { .line = 0, .character = 0 }, .end = { .line = 0, .character = 0 } };
	}

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
	SectionLocation extractLocation(const dia::term_ui_view::CodeSection& section) {
		Range range;

		range.start.line      = static_cast<Uint>(section.location.line - 1);
		range.start.character = static_cast<Uint>(section.location.column - 1);

		if (section.location.end_line.has_value() && section.location.end_column.has_value()) {
			range.end.line      = static_cast<Uint>(section.location.end_line.value() - 1);
			range.end.character = static_cast<Uint>(section.location.end_column.value());
		} else {
			range.end = range.start;
		}
		return SectionLocation{ .file  = toPhysicalPathString(section.location.file),
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
	SectionLocation getMessageLocation(
		const dia::term_ui_view::Message&          message,
		std::vector<DiagnosticRelatedInformation>& related_information,
		const EvaluationContext&                   evaluation_ctx
	) {
		std::string     content            = "";
		bool            found_code_section = false;
		SectionLocation loc{};

		for (const auto& section: message.sections) {
			variant_match(section) {
				variant_case(dia::term_ui_view::TextSection, text) { content += text + "\n"; }
				variant_case(dia::term_ui_view::CodeSection, section) {
					if (not found_code_section) {
						loc                = extractLocation(section);
						found_code_section = true;
					} else {
						if (content.empty()) continue;
						SectionLocation section_loc = extractLocation(section);

						related_information.push_back(DiagnosticRelatedInformation{
							.location = Location{ .uri   = evaluation_ctx.resolve(section_loc.file),
						                          .range = section_loc.range },
							.message  = content });
						content = "";
					}
				}
			}
		}
		if (not found_code_section)
			loc = SectionLocation{ .file  = evaluation_ctx.default_error_location,
				                   .range = wholeFileStart() };
		return loc;
	}

	/**
	 * @brief If the evaluation of the diagnostic failed, return a diagnostic
	 * indicating the failure.
	 */
	LSPDiagnosticResult failedResult(
		const std::string& error_msg, const EvaluationContext& evaluation_ctx
	) {
		Diagnostic diag;
		diag.range    = wholeFileStart();
		diag.severity = DiagnosticSeverity::Error;
		diag.source   = "Duckling";
		diag.message  = "Failed to evaluate diagnostic: " + error_msg;
		return { .diagnostic = std::move(diag),
			     .uri        = evaluation_ctx.resolve(evaluation_ctx.default_error_location) };
	}

	LSPDiagnosticResult evaluateToLanguageServerMessage(
		CRef<dia_args::Diagnostic> diagnostic_args, const EvaluationContext& evaluation_ctx
	) {
		term_ui_view::Diagnostic view;
		try {
			auto state = dia::evaluateDiagnostic(*diagnostic_args);
			view       = dia::constructTreeView(state);
		} catch (const std::exception& e) { return failedResult(e.what(), evaluation_ctx); }

		CORE_ASSERT(
			!view.messages.empty(),
			"Diagnostic view must have at least one message which is the main message."
		);

		std::vector<DiagnosticRelatedInformation> related_information;

		const auto&     main_msg = view.messages[0];
		SectionLocation loc = getMessageLocation(main_msg, related_information, evaluation_ctx);

		Diagnostic diag;
		diag.range    = loc.range;
		diag.severity = convertSeverity(main_msg.type);
		diag.code     = static_cast<::lsp::Int>(main_msg.code);
		diag.source   = "Duckling";
		diag.message  = main_msg.header;

		for (u64 i = 1; i < view.messages.size(); i++) {
			const auto& msg = view.messages[i];

			// These are code sections from a single message, useful when the message has
			// multiple locations.
			std::vector<DiagnosticRelatedInformation> message_related;
			SectionLocation msg_loc = getMessageLocation(msg, message_related, evaluation_ctx);

			related_information.push_back(DiagnosticRelatedInformation{
				.location
				= Location{ .uri = evaluation_ctx.resolve(msg_loc.file), .range = msg_loc.range },
				.message = msg.header });

			related_information.insert(
				related_information.end(),
				std::make_move_iterator(message_related.begin()),
				std::make_move_iterator(message_related.end())
			);
		}

		if (!related_information.empty()) diag.relatedInformation = std::move(related_information);

		return { .diagnostic = std::move(diag), .uri = evaluation_ctx.resolve(loc.file) };
	}

}
